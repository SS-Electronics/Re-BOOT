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

/**
 * @brief Bootloader FSM States
 */
typedef enum
{
    ST_CONNECT = 0,
    ST_SEND_RESET,
    ST_WAIT_PIPE_INFO,
    ST_VERIFY_HEX_ADDR,
    ST_SEND_PIPELINE,
    ST_WAIT_ACK,
    ST_WAIT_PIPELINE_CRC,
    ST_VERIFY_PIPELINE,
    ST_NEXT_PIPELINE,
    ST_ADDR_UPDATE,
    ST_START_APP,
    ST_FINISHED,
    ST_ERROR

} bl_state_t;


/**
 * @brief Bootloader FSM Events
 */
typedef enum
{
    EV_NONE = 0,
    EV_CONNECT_OK,
    EV_ACK,
    EV_NACK,
    EV_PIPE_INFO,
    EV_PIPELINE_SENT,
    EV_PIPELINE_CRC,
    EV_CRC_OK,
    EV_NEXT,
    EV_ADDR_CHANGE,
    EV_START_ACK,
    EV_FAIL

} bl_event_t;


/**
 * @brief State Function Type: Each State return the next event
 */
typedef event_t (*state_func_t)(void *ctx);





typedef struct
{
    bl_state_t state;

    hex_record_t *records;

    int32_t total_lines;

    uint32_t current_line;

    uint32_t hex_base_addr;
    uint32_t hex_end_addr;

    uint32_t target_addr;
    uint32_t sector_size;

    uint32_t pipeline_start;

    uint8_t retry;

} bl_ctx_t;


typedef struct
{
    bl_state_t state;
    bl_event_t event;
    bl_state_t next;

} fsm_transition_t;





#ifdef __cplusplus
extern "C" {
#endif



void bootloader_run(hex_record_t *records,
                    int32_t total_lines,
                    uint32_t hex_base_addr,
                    uint32_t hex_end_addr);






#ifdef __cplusplus
}
#endif


#endif /* __FSM_H__ */