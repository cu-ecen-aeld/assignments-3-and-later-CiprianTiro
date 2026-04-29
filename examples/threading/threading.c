#include "threading.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

// Optional: use these functions to add debug or error prints to your application
//#define DEBUG_LOG(msg,...)
#define DEBUG_LOG(msg,...) printf("threading: " msg "\n" , ##__VA_ARGS__)
#define ERROR_LOG(msg,...) printf("threading ERROR: " msg "\n" , ##__VA_ARGS__)

void* threadfunc(void* thread_param)
{
    DEBUG_LOG("Executing thread");
    struct thread_data* thread_func_args = (struct thread_data *) thread_param;

    //DEBUG_LOG("Thread sleeping for %d ms", thread_func_args->wait_to_obtain_ms);
    
    /* Sleep. */
    usleep(thread_func_args->wait_to_obtain_ms * 1000);
    
    //DEBUG_LOG("Hold mutex for %d ms", thread_func_args->wait_to_release_ms);
    
    pthread_mutex_lock(thread_func_args->mutex);
 
    /* Sleep. */
    usleep(thread_func_args->wait_to_release_ms * 1000);
    
    pthread_mutex_unlock(thread_func_args->mutex);

    thread_func_args->thread_complete_success = true;

    return (void*)thread_func_args;
}


bool start_thread_obtaining_mutex(pthread_t *thread, pthread_mutex_t *mutex,int wait_to_obtain_ms, int wait_to_release_ms)
{
    bool returnVal = false;

    struct thread_data *data = malloc(sizeof(struct thread_data));
    data->wait_to_obtain_ms = wait_to_obtain_ms;
    data->wait_to_release_ms = wait_to_release_ms;
    data->mutex = mutex;

    /* Start thread. */
    if ( pthread_create(thread, NULL, &threadfunc, data) != 0 )
    {
        //ERROR_LOG("FAILED to create the thread");
        free(data);
    }
    else
    {
        //DEBUG_LOG("Reaching the end of the function");
        returnVal = true;
    }
        
    return returnVal;
}

