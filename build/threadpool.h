#ifndef INCLUDE_THREADPOOL_H
#define INCLUDE_THREADPOOL_H

#include <assert.h>
#include <pthread.h>
#include <stdbool.h>

#define STATIC_ARG_MAX (4 * sizeof(void *))

static_assert(
	STATIC_ARG_MAX >= sizeof(void *),
	"STATIC_ARG_MAX must be large enough to hold a void pointer"
);

typedef void (*WorkerCb)(void *);

struct ThreadPool {
	size_t max_workers;
	pthread_mutex_t idle_list_mut;
	pthread_cond_t idle_list_cond;
	struct ThreadPoolWorker *idle_workers, *workers;
};

int
threadpool_init(
	struct ThreadPool *pool,
	size_t max_workers,
	size_t arg_size
);
void
threadpool_execute(struct ThreadPool *pool, WorkerCb fn, void *args);
void
threadpool_destroy(struct ThreadPool *pool);

#endif
