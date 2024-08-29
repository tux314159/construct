#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "threadpool.h"
#include "util.h"

struct ThreadPoolWorker {
	/* Thread info */
	pthread_t tid;
	pthread_mutex_t *idle_list_mut;
	pthread_cond_t *idle_list_cond;
	pthread_mutex_t *start_mut;
	pthread_cond_t *start_cond;
	bool start;

	/* Argument passing */
	WorkerCb fn;
	size_t arg_size;
	void *args;
	char static_args[STATIC_ARG_MAX];

	/* List */
	struct ThreadPoolWorker **head;
	struct ThreadPoolWorker *next;
};

static void *
worker_stub(void *_self)
{
	struct ThreadPoolWorker *self = _self;

	self->start = false;
	self->start_mut = xmalloc(sizeof(*self->start_mut));
	self->start_cond = xmalloc(sizeof(*self->start_cond));
	pthread_mutex_init(self->start_mut, NULL);
	pthread_cond_init(self->start_cond, NULL);
	if (self->arg_size > STATIC_ARG_MAX)
		self->args = xmalloc(self->arg_size);
	else
		self->args = self->static_args;

	for (;;) {
		pthread_mutex_lock(self->start_mut);

		pthread_mutex_lock(self->idle_list_mut);
		self->next = *self->head;
		*self->head = self;
		pthread_cond_signal(self->idle_list_cond);
		pthread_mutex_unlock(self->idle_list_mut);
		while (!self->start)
			pthread_cond_wait(self->start_cond, self->start_mut);
		self->start = false;
		pthread_mutex_unlock(self->start_mut);

		self->fn(self->args);
	}
}

static void
worker_term(void *_self)
{
	struct ThreadPoolWorker *self = *(struct ThreadPoolWorker **)_self;

	pthread_mutex_destroy(self->start_mut);
	pthread_cond_destroy(self->start_cond);
	free(self->start_mut);
	free(self->start_cond);
	if (self->args != self->static_args)
		free(self->args);
	pthread_exit(0);
}

int
threadpool_init(
	struct ThreadPool *pool,
	size_t max_workers,
	size_t arg_size
)
{
	pool->max_workers = max_workers;
	pool->idle_list_mut = (pthread_mutex_t)PTHREAD_MUTEX_INITIALIZER;
	pool->idle_list_cond = (pthread_cond_t)PTHREAD_COND_INITIALIZER;

	pool->workers = xmalloc(max_workers * sizeof(*pool->idle_workers));
	pool->idle_workers = NULL;
	for (unsigned i = 0; i < max_workers; i++) {
		pool->workers[i].arg_size = arg_size;

		pool->workers[i].head = &pool->idle_workers;
		pool->workers[i].next = NULL;
		pool->workers[i].idle_list_mut = &pool->idle_list_mut;
		pool->workers[i].idle_list_cond = &pool->idle_list_cond;

		if (pthread_create(
				&pool->workers[i].tid,
				NULL,
				worker_stub,
				pool->workers + i
			) != 0)
			die("failed to spawn thread", 0);
	}

	return 0;
}

void
threadpool_execute(struct ThreadPool *pool, WorkerCb fn, void *args)
{
	struct ThreadPoolWorker *worker;

	pthread_mutex_lock(&pool->idle_list_mut);
	while (pool->idle_workers == NULL)
		pthread_cond_wait(&pool->idle_list_cond, &pool->idle_list_mut);
	worker = pool->idle_workers;
	pool->idle_workers = pool->idle_workers->next;
	pthread_mutex_unlock(&pool->idle_list_mut);

	pthread_mutex_lock(worker->start_mut);
	worker->fn = fn;
	if (fn == &worker_term) /* HACK */
		memcpy(worker->args, &worker, sizeof(worker));
	else
		memcpy(worker->args, args, worker->arg_size);
	worker->start = true;
	pthread_cond_signal(worker->start_cond);
	pthread_mutex_unlock(worker->start_mut);
}

void
threadpool_destroy(struct ThreadPool *pool)
{
	for (unsigned i = 0; i < pool->max_workers; i++)
		threadpool_execute(pool, &worker_term, NULL);
	for (unsigned i = 0; i < pool->max_workers; i++)
		pthread_join(pool->workers[i].tid, NULL);

	pthread_mutex_destroy(&pool->idle_list_mut);
	pthread_cond_destroy(&pool->idle_list_cond);
	free(pool->workers);
}
