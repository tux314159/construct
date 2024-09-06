#include <stdarg.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>

#include <sys/stat.h>

#include "build.h"
#include "simpleds.h"
#include "table.h"
#include "target.h"
#include "threadpool.h"
#include "util.h"

static int
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
	if (str_len(targ->cmd))
		log(msgt_raw, "%s", targ->cmd);
	status = system(targ->cmd);
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

/* BFS from the final target, prune, and find leaves */
static inline struct Array
graph_prepare(struct Depgraph *graph, struct Target *final_targ)
{
	struct Queue queue;
	struct Array leaves;
	struct stat sb;

	queue_init(&queue, graph->n_targets);
	array_init(&leaves);

	queue_push(&queue, final_targ);
	final_targ->visited = 1;

	while (queue_len(&queue)) {
		struct Target *targ, *new;

		targ = queue_pop(&queue);
		if (targ->deps.len == 0)
			array_push(&leaves, targ);

		for (size_t i = 0; i < targ->deps.len; i++) {
			struct Target **c;
			c = (struct Target **)
				table_find(&graph->targets, targ->deps.data[i]);
			if (!c) {
				/* If a dependency doesn't exist, it may be a source file */
				if (stat(targ->deps.data[i], &sb) == -1)
					die("bad target: %s", (Str)targ->deps.data[i]);
				new = target_from_rule(make_rule(
					FRAG_CONSTRUCTOR(target)(targ->deps.data[i]),
					NULL,
					NULL,
					0
				));
				graph_add_target(graph, new);
				c = &new;
			}

			array_push(&(*c)->codeps, targ); /* fill up codeps */
			if (!(*c)->visited) {
				queue_push(&queue, *c);
				(*c)->visited = 1;
			}
		}
	}

	return leaves;
}

struct WorkerArg {
	struct Target *targ;
	atomic_bool *error;
};

static void
graph_run_target(void *_args)
{
	struct WorkerArg args = *(struct WorkerArg *)_args;
	struct Target *targ = args.targ;
	int status;

	if ((targ->deps.len == 0 || target_check_ood(targ)) &&
	    targ->cmd) { /* phony if it has no deps */
		status = target_run(targ);
		if (status) {
			log(msgt_err, "job failed with exit status %d\n", status);
			*args.error = true;
		}
	}

	for (size_t i = 0; i < targ->codeps.len; i++) {
		struct Target *c;
		c = targ->codeps.data[i];
		c->n_sat_dep++;
	}
}

void
graph_build(
	struct Depgraph *graph,
	struct Target *final_targ,
	unsigned max_jobs
)
{
	struct ThreadPool threadpool;
	struct Array leaves;
	struct Queue queue;
	atomic_bool worker_error = false;

	leaves = graph_prepare(graph, final_targ);
	threadpool_init(&threadpool, max_jobs, sizeof(struct WorkerArg));

	/* Multisource BFS from each leaf
	 * NOTE: visited is flipped */
	queue_init(&queue, graph->n_targets);
	for (size_t i = 0; i < leaves.len; i++) {
		queue_push(&queue, leaves.data[i]);
		((struct Target *)(leaves.data[i]))->visited = 0;
	}

	while (queue_len(&queue) && !worker_error) {
		struct Target *targ;
		struct WorkerArg args;

		/* For every target depending on us we keep track of the
		 * number of satisfied targets and only build it if all
		 * its targets are satisfied */
		targ = queue_pop(&queue);
		if (targ->n_sat_dep < targ->deps.len) {
			queue_push(&queue, targ);
			continue;
		}

		args.targ = targ;
		args.error = &worker_error;
		threadpool_execute(&threadpool, &graph_run_target, &args);
		for (size_t i = 0; i < targ->codeps.len; i++) {
			struct Target *c;
			c = targ->codeps.data[i];
			if (!c->visited)
				continue;
			c->visited = 0;
			queue_push(&queue, c);
		}
	}

	threadpool_destroy(&threadpool);
	queue_destroy(&queue);
	array_destroy(&leaves);
}
