#include <fcntl.h>
#include <semaphore.h>
#include <stdarg.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include "simpleds.h"
#include "table.h"
#include "util.h"
#include "build.h"

static char *
strdup_s(const char *s)
{
	char *ns;
	ns = strdup(s);
	if (!ns)
		abort();
	return ns;
}

static pid_t
fork_s(void)
{
	pid_t pid;
	pid = fork();
	if (pid == -1)
		die("error creating child process", 0);
	return pid;
}

char *
format_cmd(const char *s, const char *name, struct Array *deps)
{
	char *res_buf;
	size_t res_len;
	FILE *res;

	int special = 0, escaped = 0;

	res = open_memstream(&res_buf, &res_len);
	if (!res)
		abort();

	for (const char *c = s; c < s + strlen(s); c++) {
		if (special) {
			special = 0;
			if (escaped)
				continue;
			switch (*c) {
			case '@':
				fprintf(res, "%s", name);
				break;
			case '<':
				fprintf(res, "%s", (char *)deps->data[0]);
				break;
			case '^':
				for (size_t i = 0; i < deps->len; i++)
					fprintf(res, "%s ", (char *)deps->data[i]);
				break;
			}
		} else {
			switch (*c) {
			case '\\':
				escaped = 1;
				continue;
			case '$':
				special = 1;
				break;
			default:
				fprintf(res, "%c", *c);
				break;
			}
		}
		escaped = 0;
	}

	fclose(res);
	return res_buf;
}

struct Target *
target_make(const char *name, const char *cmd, ...)
{
	va_list args;
	char *arg;
	struct Target *target;

	target = xmalloc(sizeof(*target));
	target->name = strdup_s(name);
	array_init(&target->deps);
	array_init(&target->codeps);

	target->n_sat_dep = NULL;
	target->visited = 0;

	// These must be pushed in order!
	va_start(args, cmd);
	while ((arg = va_arg(args, char *)))
		array_push(&target->deps, arg);

	target->raw_cmd = cmd ? strdup_s(cmd) : NULL;

	return target;
}

void
target_add_dep(struct Target *parent, const char *dep_name)
{
	array_push(&parent->deps, strdup_s(dep_name));
}

int
target_check_ood(struct Target *targ)
{
	int out_of_date;
	struct stat sb;
	struct timespec targ_mtim;

	// Check if any dependencies are younger
	if (stat(targ->name, &sb) == -1) { // doesn't exist
		targ_mtim.tv_sec = 0;
		targ_mtim.tv_nsec = 0;
	} else {
#ifdef __APPLE__ // piece of shit
		targ_mtim = sb.st_mtimespec;
#else
		targ_mtim = sb.st_mtim;
#endif
	}

	out_of_date = 0;
	for (size_t i = 0; i < targ->deps.len; i++) {
		out_of_date |= stat(targ->deps.data[i], &sb) == -1;
		out_of_date |= targ_mtim.tv_sec < sb.st_mtim.tv_sec;
		out_of_date |=
			(targ_mtim.tv_sec == sb.st_mtim.tv_sec &&
		     targ_mtim.tv_nsec < sb.st_mtim.tv_nsec);
	}

	return out_of_date;
}

int
target_run(struct Target *targ)
{
	int status;
	char *cmd;
	cmd = format_cmd(targ->raw_cmd, targ->name, &targ->deps);
	log(msgt_raw, "%s", cmd);
	status = system(cmd);
	free(cmd);
	return WEXITSTATUS(status);
}

void
graph_init(struct Depgraph *graph)
{
	graph->n_targets = 0;
	table_init(&graph->targets);
}

struct Target *
graph_add_target(struct Depgraph *graph, struct Target *target)
{
	struct Target *targ_loc;
	targ_loc = table_insert(&graph->targets, target->name, target);
	graph->n_targets++;
	return targ_loc;
}

struct Target *
graph_get_target(struct Depgraph *graph, const char *targ_name)
{
	struct Target **targ;
	targ = (struct Target **)table_find(&graph->targets, targ_name);
	return targ ? *targ : NULL;
}

struct DepCnts {
	size_t dep_cnt_sz;
	atomic_size_t *dep_cnt;
};

struct DepCnts
alloc_depcnts(size_t n)
{
	struct DepCnts shm;
	int tmpfile;
	char tmpfile_name[] = "/tmp/construct-XXXXXX";

	shm.dep_cnt_sz = n * sizeof(*shm.dep_cnt);

	tmpfile = mkstemp(tmpfile_name);
	if (tmpfile == -1)
		die("error creating temporary file %s", tmpfile_name);

	shm.dep_cnt = mmap(
		NULL,
		shm.dep_cnt_sz,
		PROT_READ | PROT_WRITE,
		MAP_SHARED,
		tmpfile,
		0
	);

	unlink(tmpfile_name);

	if (ftruncate(tmpfile, (off_t)shm.dep_cnt_sz) == -1)
		die("error allocating space in temporary file", 0);
	close(tmpfile);

	if (!shm.dep_cnt)
		abort(); /* sounds like oom */

	for (atomic_size_t *c = shm.dep_cnt; c < shm.dep_cnt + n; c++)
		atomic_init(c, 0);

	return shm;
}

static void
free_depcnts(struct DepCnts shm)
{
	munmap(shm.dep_cnt, shm.dep_cnt_sz);
}

// BFS from the final target, prune, and find leaves
static struct Array
graph_prepare(
	struct Depgraph *graph,
	struct Target *final_targ,
	struct DepCnts shm
)
{
	size_t shm_idx;
	struct Queue queue;
	struct Array leaves;
	struct stat sb;

	queue_init(&queue, graph->n_targets);
	array_init(&leaves);

	queue_push(&queue, final_targ);
	final_targ->visited = 1;
	shm_idx = 0;

	while (queue_len(&queue)) {
		struct Target *targ, *new;

		targ = queue_pop(&queue);
		if (targ->deps.len == 0)
			array_push(&leaves, targ); // collate leaves
		else
			targ->n_sat_dep = shm.dep_cnt + (shm_idx++);

		for (size_t i = 0; i < targ->deps.len; i++) {
			struct Target **c;
			c = (struct Target **)
				table_find(&graph->targets, targ->deps.data[i]);
			if (!c) {
				// If sought-after dependency doesn't exist, it may
				// be a source file, try to find and add it
				if (stat(targ->deps.data[i], &sb) == -1)
					die("bad target: %s", (char *)targ->deps.data[i]);
				new = target_make(targ->deps.data[i], NULL, NULL);
				graph_add_target(graph, new);
				c = &new;
			}
			array_push(&(*c)->codeps, targ); // fill up codeps

			if (!(*c)->visited) {
				queue_push(&queue, *c);
				(*c)->visited = 1;
			}
		}
	}

	return leaves;
}

void
graph_build(struct Depgraph *graph, struct Target *final_targ, int max_jobs)
{
	struct DepCnts shm;
	struct Array leaves;
	struct Queue queue;
	int cur_jobs;

	shm = alloc_depcnts(graph->n_targets);
	leaves = graph_prepare(graph, final_targ, shm);

	// Execute build plan
	queue_init(&queue, graph->n_targets);

	// Multisource BFS from each source
	// NOTE: visited is flipped
	for (size_t i = 0; i < leaves.len; i++) {
		queue_push(&queue, leaves.data[i]); // start from each leaf
		((struct Target *)(leaves.data[i]))->visited = 0;
	}

	cur_jobs = 0;
	while (queue_len(&queue)) {
		struct Target *targ;
		pid_t pid;
		int child_status;

		// Block and wait for jobs to terminate if at max
		while (cur_jobs >= max_jobs) {
			wait(NULL);
			cur_jobs--;
		}

		while ((pid = waitpid(-1, &child_status, WNOHANG)) > 0) {
			cur_jobs--;
			if (WEXITSTATUS(child_status)) {
				die("job terminated with status %d", WEXITSTATUS(child_status));
				exit(1);
			}
		}

		// For every target depending on us we keep track of the
		// number of satisfied targets and only build it if all
		// its targets are satisfied
		targ = queue_pop(&queue);
		if (targ->n_sat_dep && *targ->n_sat_dep < targ->deps.len) {
			queue_push(&queue, targ); // push it back to the back
			continue;
		}

		if ((pid = fork_s())) {
			cur_jobs++;
			for (size_t i = 0; i < targ->codeps.len; i++) {
				struct Target *c;
				c = targ->codeps.data[i];
				if (!c->visited)
					continue;
				c->visited = 0;
				queue_push(&queue, c);
			}
		} else {
			int status;

			if ((targ->deps.len == 0 || target_check_ood(targ)) &&
			    targ->raw_cmd) { // phony if it has no deps
				status = target_run(targ);
			}

			for (size_t i = 0; i < targ->codeps.len; i++) {
				struct Target *c;
				c = targ->codeps.data[i];
				(*c->n_sat_dep)++;
			}

			_exit(status);
		}
	}

	queue_destroy(&queue);
	array_destroy(&leaves);
	free_depcnts(shm);
}

