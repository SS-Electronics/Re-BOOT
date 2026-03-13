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


#ifndef __FSM_H__
#define __FSM_H__

#include "app_types.h"
#include "pipeline.h"
#include "threads.h"
#include "queues.h"


#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    ST_INIT,
    ST_SEND_RESET,
    ST_WAIT_INFO,
    ST_BUILD_PIPELINE,
    ST_SEND_WINDOW,
    ST_WAIT_ACK,
    ST_SECTOR_DONE,
    ST_VERIFY,
    ST_NEXT_SECTOR,
    ST_APP_START,
    ST_DONE,
    ST_ERROR

} bl_state_t;

typedef enum
{
    EVT_START,
    EVT_TARGET_INFO,
    EVT_SEG_ACK,
    EVT_TIMEOUT,
    EVT_SECTOR_END,
    EVT_CRC_OK,
    EVT_APP_ACK

} bl_event_t;

typedef struct
{
    bl_event_t type;

    uint16_t sector_size;
    uint16_t segment_size;

} event_msg_t;


#define WINDOW_SIZE 8

static struct
{
    bl_state_t state;

    hex_record_t *records;
    uint32_t record_count;

    pipeline_builder_t pipeline;

    uint16_t sector_size;
    uint16_t segment_size;

    uint32_t current_sector;
    uint32_t offset;

    uint32_t in_flight;

} ctx;

typedef struct
{
    bl_state_t state;
    bl_event_t event;
    bl_state_t next;

    void (*action)(event_msg_t*);

} fsm_entry_t;


void fsm_init(hex_record_t *records,uint32_t count);
void fsm_post(event_msg_t *evt);
void fsm_run();







#ifdef __cplusplus
}
#endif


#endif /* __FSM_H__ */