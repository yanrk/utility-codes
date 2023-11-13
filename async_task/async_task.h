#ifndef ASYNC_TASK_H
#define ASYNC_TASK_H


#include <stdlib.h>

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct task task_t;

// return >=0 ? success : failure
typedef int (*work_function)(task_t * task);
typedef void (*callback_function)(task_t * task);
typedef void (*clean_function)(task_t * task);

struct task
{
    char                name[64];
    int                 line;

    work_function       work;
    void              * input;
    void              * output;
    int                 result;
    int                 error;

    callback_function   callback;
    callback_function   err_callback;
    clean_function      clean_callback;

    struct task       * next;
};

int async_task_init();
void async_task_add(task_t * task, const char * name);

#ifdef __cplusplus
}
#endif


#endif // ASYNC_TASK_H
