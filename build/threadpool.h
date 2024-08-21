#ifndef INCLUDE_THREADPOOL_H
#define INCLUDE_THREADPOOL_H

#include <pthread.h>

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

int
threadpool_init(struct ThreadPool *pool, size_t max_workers);
void
threadpool_execute(struct ThreadPool *pool, void *(*fn)(void *), void *args);
void
threadpool_destroy(struct ThreadPool *pool);

#endif
