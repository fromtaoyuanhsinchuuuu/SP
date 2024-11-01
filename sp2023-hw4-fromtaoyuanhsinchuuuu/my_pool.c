#include "my_pool.h"
#include <pthread.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

static void tpool_work_destroy(tpool_work_t *work)
{
    if (work == NULL)
        return;
    free(work);
}

tpool_work_t *tpool_work_get(tpool *threadpool) // get next thread to work
{
  if (threadpool == NULL) return NULL;
  tpool_work_t *tmp = threadpool->work_first;
  if (tmp->next == NULL){
    threadpool->work_first = NULL;
    threadpool->work_last = NULL;
  }
  else{
    threadpool->work_first = tmp->next;
  }
  return tmp;
}

static void *tpool_worker(void *arg)
{
    tpool *tmp_pool = arg;
    tpool_work_t *work;

    while (1) {
        pthread_mutex_lock(&(tmp_pool->work_mutex));

        while (tmp_pool->work_first == NULL && !tmp_pool->stop)
            pthread_cond_wait(&(tmp_pool->work_cond), &(tmp_pool->work_mutex));

        if (tmp_pool->stop) break;

        work = tpool_work_get(tmp_pool);
        tmp_pool->working_cnt++;
        pthread_mutex_unlock(&(tmp_pool->work_mutex));

        if (work != NULL) {
            work->func(work->arg);
            tpool_work_destroy(work);
        }

        pthread_mutex_lock(&(tmp_pool->work_mutex));
        tmp_pool->working_cnt--;
        if (!tmp_pool->stop && tmp_pool->working_cnt == 0 && tmp_pool->work_first == NULL) // call main thread to wake up
            pthread_cond_signal(&(tmp_pool->working_cond));
        pthread_mutex_unlock(&(tmp_pool->work_mutex));
    }

    tmp_pool->thread_cnt--;
    pthread_cond_signal(&(tmp_pool->working_cond)); // wake main

    pthread_mutex_unlock(&(tmp_pool->work_mutex));
    return NULL;
}

tpool_work_t *tpool_work_create(thread_func_t func, void *arg)
{
  tpool_work_t *now = (tpool_work_t *) malloc(sizeof(tpool_work_t));
  now->func = func, now->arg = arg, now->next = NULL;
  return now;
}


void tpool_add(tpool *pool, void *(*func)(void *), void *arg){
    if (pool == NULL) return;
    tpool_work_t *work = tpool_work_create(func, arg);

    pthread_mutex_lock(&(pool->work_mutex));

    /* add work into queue */
    if (pool->work_first == NULL){
        pool->work_first = work;
        pool->work_last  = pool->work_first;
    } else {
        pool->work_last->next = work;
        pool->work_last = work;
    }

    pthread_cond_broadcast(&(pool->work_cond));
    pthread_mutex_unlock(&(pool->work_mutex));
    return;
}

void tpool_wait(tpool *pool) {
  pthread_cond_broadcast(&(pool->work_cond));
  pthread_mutex_lock(&(pool->work_mutex));
  while (pool->work_first != NULL || pool->working_cnt > 0){ // 還有工作要做
    pthread_cond_wait(&(pool->working_cond), &(pool->work_mutex));
  }
  pool->stop = 1;
  pthread_mutex_unlock(&(pool->work_mutex));
  return;
}

void tpool_destroy(tpool *pool) 
{
  assert(pool->stop == 1 && pool->work_first == NULL && pool->working_cnt == 0);
  pthread_mutex_lock(&(pool->work_mutex));
  while(pool->thread_cnt > 0){
    pthread_cond_broadcast(&(pool->work_cond));
    pthread_cond_wait(&(pool->working_cond), &(pool->work_mutex));
  }
  pthread_mutex_unlock(&(pool->work_mutex));
  assert(pool->thread_cnt == 0 && pool->work_first == NULL && pool->stop == 1);

  pthread_mutex_destroy(&(pool->work_mutex));
  return;
}

tpool *tpool_init(int n_threads) {
  tpool *now_pool = (tpool *) malloc(sizeof(tpool));
  pthread_t tmp_thread;
  now_pool->thread_cnt = n_threads;
  pthread_mutex_init(&(now_pool->work_mutex), NULL);
  pthread_cond_init(&(now_pool->work_cond), NULL);
  pthread_cond_init(&(now_pool->working_cond), NULL);
  now_pool->work_first = NULL, now_pool->work_last = NULL, now_pool->stop = 0;

  for (int i = 0; i < n_threads; i++){
    pthread_create(&tmp_thread, NULL,  tpool_worker, now_pool);
    pthread_detach(tmp_thread);

  }
  return now_pool;
}