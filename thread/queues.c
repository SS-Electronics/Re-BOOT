/* 
File:        queues.c
Author:      Subhajit Roy  
             subhajitroy005@gmail.com 

Moudle:      Thread  
Info:        Thread Safe Queue implementation           
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
along with Re-BOOT. If not, see <https://www.gnu.org/licenses/>.
*/

/**
 * @file queues.c
 * @brief Thread-safe queue implementation.
 *
 * This file implements a portable FIFO queue designed for
 * inter-thread communication using mutexes and condition variables.
 *
 * The queue supports:
 * - Multiple producers
 * - Multiple consumers
 * - Blocking pop
 * - Non-blocking pop
 *
 * Synchronization primitives are provided by the thread abstraction
 * layer defined in threads.h.
 *
 * Typical usage pattern:
 *
 * Producer threads:
 *   queue_push()
 *
 * Consumer threads:
 *   queue_pop()
 */

#include "queues.h"


/**
 * @brief Initialize a queue.
 *
 * Initializes internal pointers and synchronization primitives.
 *
 * @param q Pointer to queue object.
 *
 * @return
 * - 0 on success
 */
int queue_init(queue_t *q)
{
    q->head = NULL;
    q->tail = NULL;
    q->size = 0;

    mutex_init(&q->mutex);
    cond_init(&q->cond);

    return 0;
}


/**
 * @brief Destroy a queue.
 *
 * Frees all remaining queue nodes and destroys the synchronization
 * primitives associated with the queue.
 *
 * @warning
 * This function must only be called when no other threads are
 * accessing the queue.
 *
 * @param q Pointer to queue object.
 */
void queue_destroy(queue_t *q)
{
    queue_node_t *n = q->head;

    while (n)
    {
        queue_node_t *tmp = n;
        n = n->next;
        free(tmp);
    }

    mutex_destroy(&q->mutex);
    cond_destroy(&q->cond);
}


/**
 * @brief Push a message into the queue.
 *
 * Adds a new node to the tail of the queue and signals a waiting
 * consumer thread if one exists.
 *
 * This function is thread-safe.
 *
 * @param q Pointer to queue.
 * @param msg Pointer to user data.
 */
void queue_push(queue_t *q, void *msg)
{
    queue_node_t *node = malloc(sizeof(queue_node_t));

    node->data = msg;
    node->next = NULL;

    mutex_lock(&q->mutex);

    if (q->tail)
        q->tail->next = node;
    else
        q->head = node;

    q->tail = node;
    q->size++;

    /* wake one waiting consumer */
    cond_signal(&q->cond);

    mutex_unlock(&q->mutex);
}


/**
 * @brief Pop a message from the queue (blocking).
 *
 * Removes the first node from the queue and returns the stored data.
 *
 * If the queue is empty the calling thread will block until
 * a producer pushes new data.
 *
 * @param q Pointer to queue.
 *
 * @return Pointer to message data.
 */
void* queue_pop(queue_t *q)
{
    mutex_lock(&q->mutex);

    /* wait until queue contains data */
    while (q->size == 0)
        cond_wait(&q->cond, &q->mutex);

    queue_node_t *node = q->head;

    q->head = node->next;

    if (!q->head)
        q->tail = NULL;

    q->size--;

    mutex_unlock(&q->mutex);

    void *data = node->data;

    free(node);

    return data;
}


/**
 * @brief Attempt to pop a message without blocking.
 *
 * If the queue contains data the first element is removed
 * and returned through the output parameter.
 *
 * If the queue is empty the function returns immediately.
 *
 * @param q Pointer to queue.
 * @param msg Output pointer receiving message data.
 *
 * @return
 * - true  : element retrieved
 * - false : queue was empty
 */
bool queue_try_pop(queue_t *q, void **msg)
{
    bool result = false;

    mutex_lock(&q->mutex);

    if (q->size > 0)
    {
        queue_node_t *node = q->head;

        q->head = node->next;

        if (!q->head)
            q->tail = NULL;

        q->size--;

        *msg = node->data;

        free(node);

        result = true;
    }

    mutex_unlock(&q->mutex);

    return result;
}


/**
 * @brief Get current queue size.
 *
 * Returns the number of elements currently stored in the queue.
 *
 * This operation is thread-safe.
 *
 * @param q Pointer to queue.
 *
 * @return Number of elements in queue.
 */
size_t queue_size(queue_t *q)
{
    size_t s;

    mutex_lock(&q->mutex);
    s = q->size;
    mutex_unlock(&q->mutex);

    return s;
}