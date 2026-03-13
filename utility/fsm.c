/* 
File:        fsm.c
Author:      Subhajit Roy  
             subhajitroy005@gmail.com 

Moudle:      Init  
Info:        Finite State Machine related functions           
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

#include "fsm.h"
#include "transport_layer.h"
#include "bl_protocol_config.h"




#include "fsm.h"
#include <stdio.h>

/**
* @brief Create a dynamically allocated FSM event
*
* Allocates memory for a new event and initializes it.
*
* @param type Event identifier
* @param data Optional user data payload
*
* @return Pointer to created event or NULL if allocation fails
*/
fsm_event_t* fsm_event_create(uint32_t type, void *data)
{
    fsm_event_t *e = malloc(sizeof(fsm_event_t));

    if (!e)
    {
        return NULL;
    } 

    e->type = type;
    e->data = data;
    e->next = NULL;

return e;
}


/**
* @brief Free an FSM event
*
* Releases memory allocated for an event.
*
* @param event Pointer to event to free
*/
void fsm_event_free(fsm_event_t *event)
{
    if (!event)
    {
        return;
    } 
    free(event);
}


/**
* @brief Initialize FSM instance
*
* Sets up the FSM runtime context including:
* * Initial state
* * Transition table
* * User data
* * Internal event queue
*
* The entry handler of the initial state is invoked automatically.
*
* @param fsm Pointer to FSM instance
* @param initial_state Initial state of the FSM
* @param table Transition table
* @param table_size Number of transitions in the table
* @param user_data Optional user context
*/
void fsm_init(fsm_t *fsm,
            fsm_state_t *initial_state,
            const fsm_transition_t *table,
            size_t table_size,
            void *user_data)
{
    if (!fsm || !initial_state || !table) 
    {
        return;
    }
    
    fsm->current_state = initial_state;
    fsm->table = table;
    fsm->table_size = table_size;
    fsm->user_data = user_data;

    /* Initialize event queue */
    fsm->queue_head = NULL;
    fsm->queue_tail = NULL;

#if(USE_THREAD_SAFE_FSM == 1)
    mutex_init(&fsm->lock);
#endif

    /* Call entry handler of initial state */
    if (initial_state->on_entry)
    {
        initial_state->on_entry(initial_state, fsm);
    }
}


/**
* @brief Push event into the internal FSM event queue
*
* This function appends the event to the tail of the queue.
*
* @param fsm FSM instance
* @param event Event to enqueue
*/
static void fsm_queue_push(fsm_t *fsm, fsm_event_t *event)
{
    if (!fsm || !event)
    {
        return;
    }
    
#if(USE_THREAD_SAFE_FSM == 1)
    mutex_lock(&fsm->lock);
#endif

    if (!fsm->queue_head)
    {
        fsm->queue_head = fsm->queue_tail = event;
    }
    else
    {
        fsm->queue_tail->next = event;
        fsm->queue_tail = event;
    }

#if(USE_THREAD_SAFE_FSM == 1)
    mutex_unlock(&fsm->lock);
#endif

}


/**
* @brief Pop event from the internal FSM queue
*
* Removes and returns the first event in the queue.
*
* @param fsm FSM instance
*
* @return Pointer to popped event or NULL if queue is empty
*/
static fsm_event_t* fsm_queue_pop(fsm_t *fsm)
{
    if (!fsm || !fsm->queue_head) 
    {
        return NULL;
    }
#if(USE_THREAD_SAFE_FSM == 1)
    mutex_lock(&fsm->lock);
#endif

    fsm_event_t *e = fsm->queue_head;

    if (e)
    {
        fsm->queue_head = e->next;
        if (!fsm->queue_head)
            fsm->queue_tail = NULL;
    }

#if(USE_THREAD_SAFE_FSM == 1)
    mutex_unlock(&fsm->lock);
#endif

    if (e)
        e->next = NULL;

    return e;
}

/**

* @brief Dispatch event to FSM
*
* The event is placed into the FSM internal queue.
* Actual processing occurs inside fsm_run().
*
* @param fsm FSM instance
* @param event Event to dispatch
*/
void fsm_dispatch(fsm_t *fsm, fsm_event_t *event)
{
    if (!fsm || !event) 
    {
        return;
    }

    fsm_queue_push(fsm, event);
}


/**
* @brief Execute exit handlers up to a specific ancestor state
*
* This function calls exit handlers starting from the current state
* up to (but not including) the specified target ancestor.
*
* @param fsm FSM instance
* @param target Ancestor state where exit traversal stops
*/
static void fsm_exit_up_to(fsm_t *fsm, fsm_state_t *target)
{
    fsm_state_t *s = fsm->current_state;

    while (s && s != target)
    {
        if (s->on_exit)
        {
            s->on_exit(s, fsm);
        }

        s = s->parent;
    }
}


/**
* @brief Execute entry handlers from ancestor down to target state
*
* Entry handlers are called in correct hierarchical order.
*
* @param fsm FSM instance
* @param from Common ancestor state
* @param to Target state
*/
static void fsm_enter_down_from(fsm_t *fsm,
                                fsm_state_t *from,
                                fsm_state_t *to)
{
    /* Build path from target to ancestor */
    fsm_state_t *stack[10];
    int depth = 0;

    fsm_state_t *s = to;

    while (s && s != from)
    {
        stack[depth++] = s;
        s = s->parent;
    }

    /* Execute entry handlers from ancestor downwards */
    for (int i = depth - 1; i >= 0; i--)
    {
        if (stack[i]->on_entry)
        stack[i]->on_entry(stack[i], fsm);
    }
}


/**
* @brief Run FSM event processing loop
*
* This function processes queued events sequentially.
*
* For each event:
* * Find matching transition
* * Execute exit handlers
* * Execute transition action
* * Execute entry handlers
* * Update current state
*
* Events are automatically freed after processing.
*
* @param fsm FSM instance
*/
void fsm_run(fsm_t *fsm)
{
    if (!fsm) 
    {
        return;
    }
    
    fsm_event_t *event;

    while ((event = fsm_queue_pop(fsm)) != NULL)
    {
        for (size_t i = 0; i < fsm->table_size; i++)
        {
            if (fsm->table[i].from == fsm->current_state &&
                fsm->table[i].event == event->type)
            {
                fsm_state_t *target = fsm->table[i].to;
                /* Find common ancestor */
                fsm_state_t *exit_target = target;
                fsm_state_t *curr = fsm->current_state;

                while (exit_target && exit_target != curr)
                    exit_target = exit_target->parent;

                /* Execute exit handlers */
                fsm_exit_up_to(fsm, exit_target);

                /* Execute transition action */
                if (fsm->table[i].action)
                    fsm->table[i].action(event, fsm);

                /* Execute entry handlers */
                fsm_enter_down_from(fsm, exit_target, target);

                /* Update current state */
                fsm->current_state = target;

                break;
            }
        }

    /* Event processing complete */
    fsm_event_free(event);
    }
}
