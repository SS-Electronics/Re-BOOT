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
along with FreeRTOS-KERNEL. If not, see <https://www.gnu.org/licenses/>.
*/

#include "queues.h"

int queue_init(queue_t *q)
{
    q->head = NULL;
    q->tail = NULL;
    q->size = 0;

    mutex_init(&q->mutex);
    cond_init(&q->cond);

    return 0;
}


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

    cond_signal(&q->cond);

    mutex_unlock(&q->mutex);
}


void* queue_pop(queue_t *q)
{
    mutex_lock(&q->mutex);

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


size_t queue_size(queue_t *q)
{
    size_t s;

    mutex_lock(&q->mutex);
    s = q->size;
    mutex_unlock(&q->mutex);

    return s;
}