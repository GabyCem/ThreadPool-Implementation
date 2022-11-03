#ifndef __THREAD_POOL__
#define __THREAD_POOL__

#include "osqueue.h"
#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>

typedef struct {
    void (*computeFunc)(void*);
    void *param;
} task;

typedef struct thread_pool
{
 OSQueue* queue;
 int numOfMaxThreads;
 int curNumOfWorkingThreads;
 pthread_cond_t cond;
 pthread_mutex_t lock;
 pthread_cond_t destroyedCond;
 pthread_t *threads;
 int beingDestroyed;
 int shouldWaitForTasks;
}ThreadPool;

void* ThreadPoolHelper(void* tp);

ThreadPool* tpCreate(int numOfThreads);

void tpDestroy(ThreadPool* threadPool, int shouldWaitForTasks);

int tpInsertTask(ThreadPool* threadPool, void (*computeFunc) (void *), void* param);

#endif
