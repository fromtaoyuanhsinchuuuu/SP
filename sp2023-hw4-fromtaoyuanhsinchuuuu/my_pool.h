#include <pthread.h>
#include <stdbool.h>
#ifndef __MY_THREAD_POOL_H
#define __MY_THREAD_POOL_H

typedef void *(*thread_func_t)(void *arg);

typedef struct tpool_work {
    thread_func_t func;
    void *arg;
    struct tpool_work *next;
} tpool_work_t;

typedef struct tpool {
  tpool_work_t *work_first;
  tpool_work_t *work_last;
  pthread_mutex_t work_mutex;
  pthread_cond_t work_cond;
  pthread_cond_t working_cond;
  int working_cnt;
  int thread_cnt;
  bool stop;
} tpool;


tpool *tpool_init(int n_threads);
void tpool_add(tpool *, void *(*func)(void *), void *);
void tpool_wait(tpool *);
void tpool_destroy(tpool *);

#endif