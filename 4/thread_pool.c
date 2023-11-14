#include "thread_pool.h"
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct thread_task {
	thread_task_f function;
	void *arg;  // fyi: should be freed by a caller

	/* PUT HERE OTHER MEMBERS */
	void *result;
	bool is_finished;
    struct thread_pool *parent_pool;  // TODO replace together with tid_idx w if not used for changing parent threads
    int tid_idx;
};

struct thread_pool {
	pthread_t *threads;

	/* PUT HERE OTHER MEMBERS */
	int max_threads_count;
};

int
thread_pool_new(int max_thread_count, struct thread_pool **pool)
{
    if (max_thread_count < 1 || max_thread_count > TPOOL_MAX_THREADS)
        return TPOOL_ERR_INVALID_ARGUMENT;
    *pool = (struct thread_pool *) malloc(sizeof(*pool));
    (*pool)->threads = (pthread_t *) calloc(max_thread_count, sizeof(pthread_t));
    memset((*pool)->threads, 0, max_thread_count*sizeof(pthread_t));  // TODO figure out what is "start thread" and validate this
    (*pool)->max_threads_count = max_thread_count;
    return 0;
}

int
thread_pool_thread_count(const struct thread_pool *pool)
{
	int count = 0;
	for (int i = 0; i < pool->max_threads_count; i++) {
	    if (pool->threads[i] != NULL)
            count++;
	}
	return count;
}

int
thread_pool_delete(struct thread_pool *pool)
{
	if (thread_pool_thread_count(pool) != 0)
        return TPOOL_ERR_HAS_TASKS;
    free(pool->threads);
    free(pool);
	return 0;
}

int
_thread_pool_first_free_place(struct thread_pool *pool)
{
    for (int i = 0; i < pool->max_threads_count; i++) {
        if (pool->threads[i] == NULL)  // TODO check activeness
            return i;
    }
    return -1;
}

int
thread_pool_push_task(struct thread_pool *pool, struct thread_task *task)
{
    int free_place = _thread_pool_first_free_place(pool);
    if (free_place == -1)
        return TPOOL_ERR_TOO_MANY_TASKS;
    pthread_t tid;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_create(&tid, &attr, task->function, task->arg);
    task->parent_pool = pool;
    task->tid_idx = free_place;
    pool->threads[free_place] = tid;
    return 0;
}

int
thread_task_new(struct thread_task **task, thread_task_f function, void *arg)
{
    *task = (struct thread_task *) malloc(sizeof(*task));
    (*task)->arg = arg;
    (*task)->function = function;
    (*task)->result = NULL;
    (*task)->parent_pool = NULL;
    (*task)->tid_idx = -1;
    return 0;
}

bool
thread_task_is_finished(const struct thread_task *task)
{
    if (task->parent_pool != NULL)  // TODO check the thread
        return false;
    return true;
}

bool
thread_task_is_running(const struct thread_task *task)
{
	/* IMPLEMENT THIS FUNCTION */
	(void)task;
	return false;
}

int
thread_task_join(struct thread_task *task, void **result)
{
    if (task->parent_pool == NULL)
        return TPOOL_ERR_TASK_NOT_PUSHED;
    //while (!thread_task_is_finished(task))
    //*result = task->result;
    pthread_join(task->parent_pool->threads[task->tid_idx], result);  // TODO replace by pthread_cond_t
    return 0;
}

#ifdef NEED_TIMED_JOIN

int
thread_task_timed_join(struct thread_task *task, double timeout, void **result)
{
	/* IMPLEMENT THIS FUNCTION */
	(void)task;
	(void)timeout;
	(void)result;
	return TPOOL_ERR_NOT_IMPLEMENTED;
}

#endif

int
thread_task_delete(struct thread_task *task)
{
    if (task->parent_pool != NULL && !thread_task_is_finished(task))
        return TPOOL_ERR_TASK_IN_POOL;
    free(task->result);
    free(task);
    return 0;
}

#ifdef NEED_DETACH

int
thread_task_detach(struct thread_task *task)
{
	/* IMPLEMENT THIS FUNCTION */
	(void)task;
	return TPOOL_ERR_NOT_IMPLEMENTED;
}

#endif
