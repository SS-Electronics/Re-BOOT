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

#include "threads.h"

#if defined(_WIN32) || defined(_WIN64)

static DWORD WINAPI win_thread_adapter(LPVOID arg)
{
    thread_func_t func = ((thread_func_t*)arg)[0];
    void *user = ((void**)arg)[1];

    free(arg);
    func(user);

    return 0;
}

#endif

int thread_create(thread_t *thread, thread_func_t func, void *arg)
{

#if defined(__linux__)

    return pthread_create(&thread->handle, NULL, func, arg);

#elif defined(_WIN32) || defined(_WIN64)

    void **wrapper = malloc(sizeof(void*) * 2);
    wrapper[0] = (void*)func;
    wrapper[1] = arg;

    thread->handle = CreateThread(
        NULL,
        0,
        win_thread_adapter,
        wrapper,
        0,
        &thread->id);

    return thread->handle ? 0 : -1;

#endif
}

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

int thread_detach(thread_t *thread)
{

#if defined(__linux__)

    return pthread_detach(thread->handle);

#elif defined(_WIN32) || defined(_WIN64)

    CloseHandle(thread->handle);
    return 0;

#endif
}

void thread_sleep(uint32_t ms)
{

#if defined(__linux__)
    usleep(ms * 1000);
#elif defined(_WIN32) || defined(_WIN64)
    Sleep(ms);
#endif

}

void thread_yield(void)
{

#if defined(__linux__)
    sched_yield();
#elif defined(_WIN32) || defined(_WIN64)
    SwitchToThread();
#endif

}

int mutex_init(mutex_t *m)
{

#if defined(__linux__)
    return pthread_mutex_init(&m->handle, NULL);
#elif defined(_WIN32) || defined(_WIN64)
    InitializeCriticalSection(&m->handle);
    return 0;
#endif

}

void mutex_lock(mutex_t *m)
{

#if defined(__linux__)
    pthread_mutex_lock(&m->handle);
#elif defined(_WIN32) || defined(_WIN64)
    EnterCriticalSection(&m->handle);
#endif

}

void mutex_unlock(mutex_t *m)
{

#if defined(__linux__)
    pthread_mutex_unlock(&m->handle);
#elif defined(_WIN32) || defined(_WIN64)
    LeaveCriticalSection(&m->handle);
#endif

}

void mutex_destroy(mutex_t *m)
{

#if defined(__linux__)
    pthread_mutex_destroy(&m->handle);
#elif defined(_WIN32) || defined(_WIN64)
    DeleteCriticalSection(&m->handle);
#endif

}

int cond_init(cond_t *c)
{

#if defined(__linux__)
    return pthread_cond_init(&c->handle, NULL);
#elif defined(_WIN32) || defined(_WIN64)
    InitializeConditionVariable(&c->handle);
    return 0;
#endif

}

void cond_wait(cond_t *c, mutex_t *m)
{

#if defined(__linux__)
    pthread_cond_wait(&c->handle, &m->handle);
#elif defined(_WIN32) || defined(_WIN64)
    SleepConditionVariableCS(&c->handle, &m->handle, INFINITE);
#endif

}

void cond_signal(cond_t *c)
{

#if defined(__linux__)
    pthread_cond_signal(&c->handle);
#elif defined(_WIN32) || defined(_WIN64)
    WakeConditionVariable(&c->handle);
#endif

}

void cond_broadcast(cond_t *c)
{

#if defined(__linux__)
    pthread_cond_broadcast(&c->handle);
#elif defined(_WIN32) || defined(_WIN64)
    WakeAllConditionVariable(&c->handle);
#endif

}

void cond_destroy(cond_t *c)
{

#if defined(__linux__)
    pthread_cond_destroy(&c->handle);
#endif

}