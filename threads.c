#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct WorkerArgs {
	void *(*fn)(void *);
	void *args;
	void *ret;
	struct ThreadPoolWorker *self;
};

struct ThreadPoolWorker {
	pthread_t tid;
	pthread_mutex_t *idle_list_mut;
	pthread_cond_t *idle_list_cond;
	pthread_mutex_t *start_mut;
	pthread_cond_t *start_cond;

	struct WorkerArgs args;

	struct ThreadPoolWorker **head;
	struct ThreadPoolWorker *next;
};

struct ThreadPool {
	size_t max_workers;
	pthread_mutex_t idle_list_mut;
	pthread_cond_t idle_list_cond;
	struct ThreadPoolWorker *idle_workers, *workers;
};

void *
threadpool_wrk_term(void *_)
{
	pthread_exit(0);
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
	pthread_cond_signal(worker->start_cond);
	pthread_mutex_unlock(worker->start_mut);
}

void
threadpool_destroy(struct ThreadPool *pool)
{
	for (unsigned i = 0; i < pool->max_workers; i++)
		threadpool_execute(pool, &threadpool_wrk_term, NULL);
	for (unsigned i = 0; i < pool->max_workers; i++)
		pthread_join(pool->workers[i].tid, NULL);
}

void *
worker_stub(void *_args)
{
	struct WorkerArgs *args = _args;
	void *ret;

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

int
threadpool_init(struct ThreadPool *pool, size_t max_workers)
{
	pool->max_workers = max_workers;
	pool->idle_list_mut = (pthread_mutex_t)PTHREAD_MUTEX_INITIALIZER;
	pool->idle_list_cond = (pthread_cond_t)PTHREAD_COND_INITIALIZER;

	/* Create pool->workers */
	pool->workers = malloc(max_workers * sizeof(*pool->idle_workers));
	pool->idle_workers = NULL;
	for (unsigned i = 0; i < max_workers; i++) {
		pool->workers[i].args.self = pool->workers + i;

		pool->workers[i].head = &pool->idle_workers;
		pool->workers[i].next = NULL;
		pool->workers[i].start_mut = malloc(sizeof(*pool->workers[i].start_mut));
		pool->workers[i].start_cond = malloc(sizeof(*pool->workers[i].start_cond));
		pool->workers[i].idle_list_mut = &pool->idle_list_mut;
		pool->workers[i].idle_list_cond = &pool->idle_list_cond;

		pthread_mutex_init(pool->workers[i].start_mut, NULL);
		pthread_cond_init(pool->workers[i].start_cond, NULL);

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

void *
say_hello(void *_)
{
	puts("Hello world!");
	return NULL;
}

int
main(void)
{
	struct ThreadPool pool;
	threadpool_init(&pool, 5);
	for (int i = 0; i < 20; i++)
		threadpool_execute(&pool, &say_hello, NULL);
	threadpool_destroy(&pool);
} 
