#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include "stuck_check.h"
#include "async_task.h"

// 0: none
// 1: error
// 2: error and output
#define LOG_LEVEL 2

#if (LOG_LEVEL > 0)
    #define RUN_LOG_ERR(fmt, ...)   fprintf(stderr, fmt "\n", ##__VA_ARGS__)
#else
    #define RUN_LOG_ERR(fmt, ...)
#endif // (LOG_LEVEL > 0)

#if (LOG_LEVEL > 1)
    #define RUN_LOG_DBG(fmt, ...)   fprintf(stdout, fmt "\n", ##__VA_ARGS__)
#else
    #define RUN_LOG_DBG(fmt, ...)
#endif // (LOG_LEVEL > 1)

// 0: none
// 1: print out
// 2: stuck check
#define ASYNC_CHECK 1

#if (ASYNC_CHECK > 1)
    #define ASYNC_TASK_BEG(file, name, line)    stuck_checker_in(file, name, line, 0)
    #define ASYNC_TASK_END(file, name, line)    stuck_checker_out()
#elif (ASYNC_CHECK > 0)
    #define ASYNC_TASK_BEG(file, name, line)    fprintf(stdout, "async task (%s:%s:%d) begin\n", file, name, line);
    #define ASYNC_TASK_END(file, name, line)    fprintf(stdout, "async task (%s:%s:%d) end\n", file, name, line);
#else
    #define ASYNC_TASK_BEG(file, name, line)
    #define ASYNC_TASK_END(file, name, line)
#endif // (ASYNC_CHECK > 0)

typedef struct task_queue_t
{
    task_t        * head;
    task_t        * tail;
    pthread_mutex_t mutex;
    pthread_cond_t  condition;
} task_queue_t;

#define TASK_QUEUE_COUNT    1
#define TASK_THREAD_COUNT   2

static int s_task_running = 0;
static int s_task_threads = 0;
static task_queue_t s_task_queue[TASK_QUEUE_COUNT] = { 0x0 };

static void * task_work_loop(void * param)
{
    task_t * task = NULL;

    char name[sizeof(task->name)] = { 0x0 };
    int line = 0;

    task_queue_t * task_queue = (task_queue_t *)param;

    while (0 != s_task_running)
    {
        task = NULL;

        pthread_mutex_lock(&task_queue->mutex);
        while (0 != s_task_running && NULL == task_queue->head)
        {
            pthread_cond_wait(&task_queue->condition, &task_queue->mutex);
        }
        if (NULL != task_queue->head)
        {
            if (task_queue->tail == task_queue->head)
            {
                task_queue->tail = NULL;
            }
            task = task_queue->head;
            task_queue->head = task->next;
            task->next = NULL;
        }
        pthread_mutex_unlock(&task_queue->mutex);

        if (NULL != task)
        {
            strncpy(name, task->name, sizeof(name) - 1);
            line = task->line;

            if (NULL != task->work)
            {
                ASYNC_TASK_BEG(name, "work", line);
                task->result = (*task->work)(task);
                ASYNC_TASK_END(name, "work", line);
            }

            if (task->result >= 0)
            {
                if (NULL != task->callback)
                {
                    ASYNC_TASK_BEG(name, "callback", line);
                    (*task->callback)(task);
                    ASYNC_TASK_END(name, "callback", line);
                }
            }
            else
            {
                if (NULL != task->err_callback)
                {
                    ASYNC_TASK_BEG(name, "err_callback", line);
                    (*task->err_callback)(task);
                    ASYNC_TASK_END(name, "err_callback", line);
                }
            }

            if (NULL != task->clean_callback)
            {
                ASYNC_TASK_BEG(name, "clean_callback", line);
                (*task->clean_callback)(task);
                ASYNC_TASK_END(name, "clean_callback", line);
            }

            ASYNC_TASK_BEG(name, "free", line);
            if (NULL != task->input)
            {
                free(task->input);
            }
            if (NULL != task->output)
            {
                free(task->output);
            }
            free(task);
            ASYNC_TASK_END(name, "free", line);
        }
    }
}

int async_task_init()
{
    RUN_LOG_DBG("async task init begin");

    while (s_task_threads < TASK_THREAD_COUNT)
    {
        task_queue_t * task_queue = &s_task_queue[s_task_threads % TASK_QUEUE_COUNT];

        pthread_attr_t thread_attr;
        int result = pthread_attr_init(&thread_attr);
        if (0 != result)
        {
            RUN_LOG_ERR("async task init failure while init thread (%d) attr error (%d)", s_task_threads, errno);
            return (0 == s_task_threads ? -1 : 0);
        }

        result = pthread_attr_setdetachstate(&thread_attr, PTHREAD_CREATE_DETACHED);
        if (0 != result)
        {
            RUN_LOG_ERR("async task init failure while set thread (%d) detach error (%d)", s_task_threads, errno);
            return (0 == s_task_threads ? -2 : 0);
        }

        pthread_t thread_id;
        result = pthread_create(&thread_id, &thread_attr, task_work_loop, task_queue);
        if (0 != result)
        {
            RUN_LOG_ERR("async task init failure while create thread (%d) error (%d)", s_task_threads, errno);
            return (0 == s_task_threads ? -3 : 0);
        }

        if (s_task_threads < TASK_QUEUE_COUNT)
        {
            pthread_mutex_init(&task_queue->mutex, NULL);
            pthread_cond_init(&task_queue->condition, NULL);
        }

        ++s_task_threads;
    }

    s_task_running = 1;

    RUN_LOG_DBG("async task init success");

    return 0;
}

void async_task_add(task_t * task, const char * name)
{
    if (0 == s_task_running || s_task_threads <= 0 || NULL == task)
    {
        return;
    }

    if (NULL == name)
    {
        name = "";
    }

    int line = 0;

    RUN_LOG_DBG("add task (%s) begin", name);

    strncpy(task->name, name, sizeof(task->name) - 1);
    task->line = line++;

    task_queue_t * task_queue = &s_task_queue[((unsigned int)(pthread_self())) % TASK_QUEUE_COUNT];

    pthread_mutex_lock(&task_queue->mutex);
    if (NULL != task_queue->tail)
    {
        task_queue->tail->next = task;
    }
    else
    {
        task_queue->head = task;
    }
    while (NULL != task->next)
    {
        task = task->next;
        strncpy(task->name, name, sizeof(task->name) - 1);
        task->line = line++;
    }
    task_queue->tail = task;
    pthread_cond_signal(&task_queue->condition);
    pthread_mutex_unlock(&task_queue->mutex);

    RUN_LOG_DBG("add task (%s) end", name);
}
