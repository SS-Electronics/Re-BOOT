/* 
File:        threads.c
Author:      Subhajit Roy  
             subhajitroy005@gmail.com 

Moudle:      Thread  
Info:        Multithreading related operations and APIs           
Dependency:  None

This file is part of Re-BOOT Project.

Re-BOOT is free software: you can redistribute it and/or 
modify it under the terms of the GNU General Public License 
as published by the Free Software Foundation, either version 
3 of the License, or (at your option) any later version.

Re-BOOT is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of 
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the 
GNU General Public License for more details.

You should have received a copy of the GNU General Public License 
along with FreeRTOS-KERNEL. If not, see <https://www.gnu.org/licenses/>.
*/

/**
 * @file threads.c
 * @brief Cross-platform thread abstraction implementation.
 *
 * This file implements the portable thread API defined in threads.h.
 * The implementation internally maps the API to:
 *
 * - POSIX Threads (pthread) on Linux
 * - Win32 Thread API on Windows
 *
 * The goal is to provide a small, dependency-free threading layer
 * usable in system software, embedded frameworks, and infrastructure
 * components.
 */

#include "threads.h"

#if defined(_WIN32) || defined(_WIN64)

/**
 * @brief Windows thread adapter.
 *
 * Windows threads expect the entry function signature:
 *
 * @code
 * DWORD WINAPI func(LPVOID arg)
 * @endcode
 *
 * while this library exposes:
 *
 * @code
 * void* func(void *arg)
 * @endcode
 *
 * This adapter converts the user function to the Win32 compatible
 * thread entry point.
 *
 * @param arg Pointer to wrapper structure containing:
 *            - user thread function
 *            - user argument
 *
 * @return Thread exit code (always 0).
 */
static DWORD WINAPI win_thread_adapter(LPVOID arg)
{
    thread_func_t func = ((thread_func_t*)arg)[0];
    void *user = ((void**)arg)[1];

    free(arg);

    func(user);

    return 0;
}

#endif


/**
 * @brief Create a new thread.
 *
 * Starts execution of a new thread that begins at the specified
 * entry function.
 *
 * @param thread Pointer to thread object.
 * @param func Thread entry function.
 * @param arg Argument passed to the thread function.
 *
 * @return
 * - 0 on success
 * - negative value on failure
 */
int thread_create(thread_t *thread, thread_func_t func, void *arg)
{

#if defined(__linux__)

    if (!thread || !func)
    {
        return -1;
    }
    else
    {
        return pthread_create(&thread->handle, NULL, func, arg);
    }
    
#elif defined(_WIN32) || defined(_WIN64)

    /* allocate wrapper for function + argument */
    void **wrapper = malloc(sizeof(void*) * 2);

    wrapper[0] = (void*)func;
    wrapper[1] = arg;

    if (!thread || !func || !wrapper)
    {
        return -1;
    }
    else
    {
        thread->handle = CreateThread(
        NULL,
        0,
        win_thread_adapter,
        wrapper,
        0,
        &thread->id);

    return thread->handle ? 0 : -1;
    }

#endif
}


/**
 * @brief Wait for a thread to terminate.
 *
 * Blocks the calling thread until the specified thread exits.
 *
 * @param thread Pointer to thread object.
 *
 * @return
 * - 0 on success
 * - negative value on failure
 */
int thread_join(thread_t *thread)
{

#if defined(__linux__)

    return pthread_join(thread->handle, NULL);

#elif defined(_WIN32) || defined(_WIN64)

    WaitForSingleObject(thread->handle, INFINITE);
    CloseHandle(thread->handle);

    return 0;

#endif
}


/**
 * @brief Detach a thread.
 *
 * A detached thread releases its resources automatically when it
 * terminates. After detaching, the thread cannot be joined.
 *
 * @param thread Pointer to thread object.
 *
 * @return
 * - 0 on success
 */
int thread_detach(thread_t *thread)
{

#if defined(__linux__)

    return pthread_detach(thread->handle);

#elif defined(_WIN32) || defined(_WIN64)

    CloseHandle(thread->handle);

    return 0;

#endif
}


/**
 * @brief Sleep the current thread.
 *
 * Suspends execution of the calling thread for a specified duration.
 *
 * @param ms Sleep duration in milliseconds.
 */
void thread_sleep(uint32_t ms)
{

#if defined(__linux__)
    usleep(ms * 1000);
#elif defined(_WIN32) || defined(_WIN64)
    Sleep(ms);
#endif

}


/**
 * @brief Yield CPU to another thread.
 *
 * Causes the current thread to voluntarily give up the processor
 * so another runnable thread may execute.
 */
void thread_yield(void)
{

#if defined(__linux__)
    sched_yield();
#elif defined(_WIN32) || defined(_WIN64)
    SwitchToThread();
#endif

}


/**
 * @brief Initialize a mutex.
 *
 * @param m Pointer to mutex object.
 *
 * @return
 * - 0 on success
 * - negative value on failure
 */
int mutex_init(mutex_t *m)
{

#if defined(__linux__)
    return pthread_mutex_init(&m->handle, NULL);
#elif defined(_WIN32) || defined(_WIN64)
    InitializeCriticalSection(&m->handle);
    return 0;
#endif

}


/**
 * @brief Lock a mutex.
 *
 * Blocks until the mutex becomes available.
 *
 * @param m Pointer to mutex object.
 */
void mutex_lock(mutex_t *m)
{

#if defined(__linux__)
    pthread_mutex_lock(&m->handle);
#elif defined(_WIN32) || defined(_WIN64)
    EnterCriticalSection(&m->handle);
#endif

}


/**
 * @brief Unlock a mutex.
 *
 * Releases a previously locked mutex.
 *
 * @param m Pointer to mutex object.
 */
void mutex_unlock(mutex_t *m)
{

#if defined(__linux__)
    pthread_mutex_unlock(&m->handle);
#elif defined(_WIN32) || defined(_WIN64)
    LeaveCriticalSection(&m->handle);
#endif

}


/**
 * @brief Destroy a mutex.
 *
 * Releases resources associated with the mutex.
 *
 * @param m Pointer to mutex object.
 */
void mutex_destroy(mutex_t *m)
{

#if defined(__linux__)
    pthread_mutex_destroy(&m->handle);
#elif defined(_WIN32) || defined(_WIN64)
    DeleteCriticalSection(&m->handle);
#endif

}


/**
 * @brief Initialize a condition variable.
 *
 * @param c Pointer to condition variable object.
 *
 * @return
 * - 0 on success
 * - negative value on failure
 */
int cond_init(cond_t *c)
{

#if defined(__linux__)
    return pthread_cond_init(&c->handle, NULL);
#elif defined(_WIN32) || defined(_WIN64)
    InitializeConditionVariable(&c->handle);
    return 0;
#endif

}


/**
 * @brief Wait on a condition variable.
 *
 * Atomically releases the mutex and blocks the calling thread until
 * the condition variable is signaled.
 *
 * @param c Pointer to condition variable.
 * @param m Pointer to associated mutex.
 */
void cond_wait(cond_t *c, mutex_t *m)
{

#if defined(__linux__)
    pthread_cond_wait(&c->handle, &m->handle);
#elif defined(_WIN32) || defined(_WIN64)
    SleepConditionVariableCS(&c->handle, &m->handle, INFINITE);
#endif

}


/**
 * @brief Signal one waiting thread.
 *
 * Wakes a single thread waiting on the condition variable.
 *
 * @param c Pointer to condition variable.
 */
void cond_signal(cond_t *c)
{

#if defined(__linux__)
    pthread_cond_signal(&c->handle);
#elif defined(_WIN32) || defined(_WIN64)
    WakeConditionVariable(&c->handle);
#endif

}


/**
 * @brief Wake all waiting threads.
 *
 * Broadcasts a signal to all threads waiting on the condition variable.
 *
 * @param c Pointer to condition variable.
 */
void cond_broadcast(cond_t *c)
{

#if defined(__linux__)
    pthread_cond_broadcast(&c->handle);
#elif defined(_WIN32) || defined(_WIN64)
    WakeAllConditionVariable(&c->handle);
#endif

}


/**
 * @brief Destroy condition variable.
 *
 * Releases resources associated with the condition variable.
 *
 * @note
 * Windows condition variables do not require explicit destruction.
 *
 * @param c Pointer to condition variable.
 */
void cond_destroy(cond_t *c)
{

#if defined(__linux__)
    pthread_cond_destroy(&c->handle);
#endif

}