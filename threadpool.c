#include "threadpool.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

// Step 8: Implementing the Thread Pool

threadpool* create_threadpool(int num_threads_in_pool, int max_queue_size) {
    if (num_threads_in_pool <= 0 || num_threads_in_pool > MAXT_IN_POOL || max_queue_size <= 0 || max_queue_size > MAXW_IN_QUEUE) {
        fprintf(stderr, "Invalid threadpool parameters\n");
        return NULL;
    }

    threadpool *pool = (threadpool *)malloc(sizeof(threadpool));
    if (pool == NULL) {
        perror("malloc");
        return NULL;
    }

    pool->threads = (pthread_t *)malloc(sizeof(pthread_t) * num_threads_in_pool);
    pool->qhead = NULL;
    pool->qtail = NULL;
    if (pool->threads == NULL) {
        perror("malloc");
        free(pool);
        return NULL;
    }

    pool->num_threads = num_threads_in_pool;
    pool->max_qsize = max_queue_size;
    pool->qsize = 0;
    pool->shutdown = 0;
    pool->dont_accept = 0;

    if (pthread_mutex_init(&(pool->qlock), NULL) != 0 ||
        pthread_cond_init(&(pool->q_not_empty), NULL) != 0 ||
        pthread_cond_init(&(pool->q_empty), NULL) != 0 ||
        pthread_cond_init(&(pool->q_not_full), NULL) != 0) {
        perror("pthread_mutex_init or pthread_cond_init");
        free(pool->threads);
        free(pool);
        return NULL;
    }

    for (int i = 0; i < num_threads_in_pool; i++) {
        if (pthread_create(&(pool->threads[i]), NULL, do_work, (void *)pool) != 0) {
            perror("pthread_create");
            free(pool->threads);
            free(pool);
            return NULL;
        }
    }

    return pool;
}

void dispatch(threadpool *from_me, dispatch_fn dispatch_to_here, void *arg) {
    work_t *new_work = (work_t *)malloc(sizeof(work_t));
    if (new_work == NULL) {
        perror("malloc");
        return;
    }

    new_work->routine = dispatch_to_here;
    new_work->arg = arg;
    new_work->next = NULL;

    pthread_mutex_lock(&(from_me->qlock));

    while (from_me->dont_accept || from_me->qsize == from_me->max_qsize) {
        pthread_cond_wait(&(from_me->q_not_full), &(from_me->qlock));
    }

    if (from_me->qhead == NULL) {
        from_me->qhead = new_work;
        from_me->qtail = new_work;
    } else {
        from_me->qtail->next = new_work;
        from_me->qtail = new_work;
    }
    from_me->qsize++;

    pthread_cond_signal(&(from_me->q_not_empty));
    pthread_mutex_unlock(&(from_me->qlock));
}

void destroy_threadpool(threadpool *destroyme) {
    pthread_mutex_lock(&(destroyme->qlock));
    destroyme->dont_accept = 1;

    while (destroyme->qsize > 0) {
        pthread_cond_wait(&(destroyme->q_empty), &(destroyme->qlock));
    }

    destroyme->shutdown = 1;
    pthread_cond_broadcast(&(destroyme->q_not_empty));
    pthread_mutex_unlock(&(destroyme->qlock));

    for (int i = 0; i < destroyme->num_threads; i++) {
        pthread_join(destroyme->threads[i], NULL);
    }

    free(destroyme->threads);
    while (destroyme->qhead != NULL) {
        work_t *tmp = destroyme->qhead;
        destroyme->qhead = tmp->next;
        free(tmp);
    }
    free(destroyme);

    pthread_mutex_destroy(&(destroyme->qlock));
    pthread_cond_destroy(&(destroyme->q_not_empty));
    pthread_cond_destroy(&(destroyme->q_empty));
    pthread_cond_destroy(&(destroyme->q_not_full));
}

void *do_work(void *arg) {
    threadpool *pool = (threadpool *)arg;

    while (1) {
        pthread_mutex_lock(&(pool->qlock));

        while (pool->qsize == 0 && !pool->shutdown) {
            pthread_cond_wait(&(pool->q_not_empty), &(pool->qlock));
        }

        if (pool->shutdown) {
            pthread_mutex_unlock(&(pool->qlock));
            break;
        }

        work_t *work = pool->qhead;
        pool->qhead = work->next;
        pool->qsize--;

        if (pool->qsize == 0) {
            pthread_cond_signal(&(pool->q_empty));
        }

        pthread_cond_signal(&(pool->q_not_full));
        pthread_mutex_unlock(&(pool->qlock));

        work->routine(work->arg);
        free(work);
    }

    pthread_exit(NULL);
    return NULL;
}
