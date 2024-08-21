#include <pthread.h>
#include <stdlib.h>

#include "threadpool.h"

static void *
worker_stub(void *_args)
{
	struct WorkerArgs *args = _args;
	void *ret;

	args->self->start_mut = malloc(sizeof(*args->self->start_mut));
	args->self->start_cond = malloc(sizeof(*args->self->start_cond));
	pthread_mutex_init(args->self->start_mut, NULL);
	pthread_cond_init(args->self->start_cond, NULL);

	for (;;) {
		pthread_mutex_lock(args->self->start_mut);

		pthread_mutex_lock(args->self->idle_list_mut);
		args->self->next = *args->self->head;
		*args->self->head = args->self;
		pthread_cond_signal(args->self->idle_list_cond);
		pthread_mutex_unlock(args->self->idle_list_mut);
		pthread_cond_wait(args->self->start_cond, args->self->start_mut);
		pthread_mutex_unlock(args->self->start_mut);

		ret = args->fn(args->args);
		args->ret = ret;
	}
}

static void *
worker_term(void *_self)
{
	struct ThreadPoolWorker *self = _self;

	pthread_mutex_destroy(self->start_mut);
	pthread_cond_destroy(self->start_cond);
	free(self->start_mut);
	free(self->start_cond);
	pthread_exit(0);
}

int
threadpool_init(struct ThreadPool *pool, size_t max_workers)
{
	pool->max_workers = max_workers;
	pool->idle_list_mut = (pthread_mutex_t)PTHREAD_MUTEX_INITIALIZER;
	pool->idle_list_cond = (pthread_cond_t)PTHREAD_COND_INITIALIZER;

	pool->workers = malloc(max_workers * sizeof(*pool->idle_workers));
	pool->idle_workers = NULL;
	for (unsigned i = 0; i < max_workers; i++) {
		pool->workers[i].args.self = pool->workers + i;

		pool->workers[i].head = &pool->idle_workers;
		pool->workers[i].next = NULL;
		pool->workers[i].idle_list_mut = &pool->idle_list_mut;
		pool->workers[i].idle_list_cond = &pool->idle_list_cond;

		if (pthread_create(
				&pool->workers[i].tid,
				NULL,
				worker_stub,
				&pool->workers[i].args
			) != 0)
			exit(1); /* ERROR */
	}

	return 0;
}

void
threadpool_execute(struct ThreadPool *pool, void *(*fn)(void *), void *args)
{
	struct ThreadPoolWorker *worker;

	pthread_mutex_lock(&pool->idle_list_mut);
	while (pool->idle_workers == NULL)
		pthread_cond_wait(&pool->idle_list_cond, &pool->idle_list_mut);
	worker = pool->idle_workers;
	pool->idle_workers = pool->idle_workers->next;
	pthread_mutex_unlock(&pool->idle_list_mut);

	pthread_mutex_lock(worker->start_mut);
	worker->args.fn = fn;
	worker->args.args = args;
	if (fn == &worker_term) /* HACK */
		worker->args.args = worker;
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
