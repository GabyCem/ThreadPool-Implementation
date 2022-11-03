#include "threadPool.h"

// this function will handle all the tasks.
void *ThreadPoolHelper(void *Threadpool)
{
    ThreadPool *tp = (ThreadPool *)Threadpool;
    task *t;

    // while the thread is running and did not exit.
    while (1)
    {
        // we lock the threadpool.
        pthread_mutex_lock(&tp->lock);
        // while the queue is empty (there isn't a task waiting to be processed
        // and the threadpool was not called to be destroyed.
        if (tp->curNumOfWorkingThreads == 0 && !(tp->beingDestroyed))
        {
            pthread_cond_wait(&(tp->cond), &(tp->lock));
        }
        // this is in case we were given a pramater other then 0 in the destroy method.
        // so we forcefully end the tasks.
        if (tp->beingDestroyed && !tp->shouldWaitForTasks)
        {
            pthread_mutex_unlock(&tp->lock);
            break;
        }

        if (tp->shouldWaitForTasks && tp->curNumOfWorkingThreads == 0) {
            pthread_cond_signal(&(tp->destroyedCond));
            pthread_mutex_unlock(&tp->lock);
            break;
        }

        // if there are still tasks in the queue

        t = osDequeue(tp->queue);
        // if there is a task to be given.
        if (t != NULL)
        {
            t->computeFunc(t->param);
            free(t);
        }
        tp->curNumOfWorkingThreads--;

        // if the destroy function was called but it wanted to wait for running tasks.
        // the function will break from the loop only after processing the task.
        // ** note that the "shouldWaitForTasks" condition doesn't need to be checked.
        // ** this is because it would have entered the if statement before.
        // after the processing of the task.
        pthread_cond_broadcast(&(tp->cond));
        pthread_mutex_unlock(&(tp->lock));
    }
}

// this function creates an empty thread pool.
ThreadPool *tpCreate(int numOfThreads)
{
    if (numOfThreads <= 0)
    {
        perror("Wrong number of threads");
        exit(-1);
    }
    ThreadPool *tp = (ThreadPool *)malloc(sizeof(ThreadPool));
    if (tp == NULL)
    {
        perror("ThreadPool allocation is failed\n");
        return NULL;
    }
    tp->queue = osCreateQueue();
    tp->beingDestroyed = 0;
    tp->numOfMaxThreads = numOfThreads;
    tp->curNumOfWorkingThreads = 0;
    tp->shouldWaitForTasks = 1;

    if (pthread_mutex_init(&(tp->lock), NULL) != 0)
    {
        perror("Mutex failed");
        exit(-1);
    }
    if (pthread_cond_init(&(tp->cond), NULL) != 0)
    {
        perror("Cond_init failed");
        exit(-1);
    }
    if (pthread_cond_init(&(tp->destroyedCond), NULL) != 0)
    {
        perror("destroyedCond_init failed");
        exit(-1);
    }
    int i;
    tp->threads = (pthread_t *)malloc(sizeof(pthread_t) * numOfThreads);
    if (tp->threads == NULL)
    {
        perror("Allocation of threads failed");
        exit(-1);
    }
    for (i = 0; i < numOfThreads; i++)
    {
        pthread_create(&(tp->threads[i]), NULL, ThreadPoolHelper, tp);
    }

    // initialize tasks.
    return tp;
}

// this function inserts a new thread to the thread pool.
int tpInsertTask(ThreadPool *threadPool, void (*computeFunc)(void *), void *param)
{
    if (threadPool == NULL || computeFunc == NULL)
    {
        perror("The threadpool or the function are invalid");
        exit(-1);
    }
    // if the threadpool is being destroyed it will not insert the new task.
    pthread_mutex_lock(&(threadPool->lock));
    if (threadPool->beingDestroyed)
    {
        pthread_mutex_unlock(&(threadPool->lock));
        return -1;
    }
    pthread_mutex_unlock(&(threadPool->lock));
    // allocating space on the heap for the task.
    task *t = (task *)malloc(sizeof(task));
    if (t == NULL)
    {
        perror("Task malloc failed");
        exit(-1);
    }
    t->computeFunc = computeFunc;
    t->param = param;
    pthread_mutex_lock(&(threadPool->lock));
    osEnqueue((threadPool->queue), t);
    threadPool->curNumOfWorkingThreads++;
    pthread_cond_broadcast(&(threadPool->cond));
    pthread_mutex_unlock(&(threadPool->lock));
    return 0;
}

void tpDestroy(ThreadPool *threadPool, int shouldWaitForTasks)
{
    // if there is no threadpool.
    if (threadPool == NULL)
    {
        perror("Threadpool does not exist");
        return;
    }
    pthread_mutex_lock(&(threadPool->lock));
    threadPool->beingDestroyed = 1;
    threadPool->shouldWaitForTasks = shouldWaitForTasks;
    pthread_cond_broadcast(&threadPool->cond);
    // if the ShouldWaitForTasks flag is off.
    if (shouldWaitForTasks == 0)
    {
        threadPool->shouldWaitForTasks = 0;

        // while the queue is not empty.
        while (!osIsQueueEmpty(threadPool->queue))
        {
            // get the task from the queue.
            task *t = osDequeue(threadPool->queue);
            // if the task is not empty destroy it.
            while (t != NULL)
            {
                free(t);
            }
        }
    }
    // wait for all tasks that are running and in queue to be processed.
    else
    {
        pthread_cond_wait(&(threadPool->destroyedCond), &(threadPool->lock));
    }
    // free the threads array.
    if (threadPool->threads != NULL)
    {
        free(threadPool->threads);
    }
    osDestroyQueue(threadPool->queue);
    pthread_cond_destroy(&(threadPool->cond));
    pthread_cond_destroy(&(threadPool->destroyedCond));
    free(threadPool);
    pthread_mutex_destroy(&(threadPool->lock));
}