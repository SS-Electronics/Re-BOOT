/* 
File:        fsm_table.h
Author:      Subhajit Roy  
             subhajitroy005@gmail.com 

Moudle:      Init  
Info:        Finite State Machine Chart           
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


#ifndef __FSM_TABLE_H__
#define __FSM_TABLE_H__

#include "app_config.h"
#include "app_types.h"
#include "fsm.h"


typedef enum
{
    ST_INIT = 0,
    ST_SEND_RESET,
    ST_BUILD_PIPELINE,
    ST_SEND_WINDOW,
    ST_VERIFY,
    ST_NEXT_SECTOR,
    ST_DONE
} state_id_t;

typedef enum
{
    EVT_START = 1,
    EVT_TARGET_INFO,
    EVT_SEG_ACK,
    EVT_SECTOR_END,
    EVT_CRC_OK,
    EVT_APP_ACK
} event_id_t;


#ifdef __cplusplus
extern "C" {
#endif


fsm_state_t ST_INIT_STATE        = {ST_INIT, NULL, NULL, NULL};
fsm_state_t ST_SEND_RESET_STATE  = {ST_SEND_RESET, NULL, NULL, NULL};
fsm_state_t ST_BUILD_PIPE_STATE  = {ST_BUILD_PIPELINE, NULL, NULL, NULL};
fsm_state_t ST_SEND_WINDOW_STATE = {ST_SEND_WINDOW, NULL, NULL, NULL};
fsm_state_t ST_VERIFY_STATE      = {ST_VERIFY, NULL, NULL, NULL};
fsm_state_t ST_NEXT_SECTOR_STATE = {ST_NEXT_SECTOR, NULL, NULL, NULL};
fsm_state_t ST_DONE_STATE        = {ST_DONE, NULL, NULL, NULL};


extern fsm_transition_t fsm_table[] =
{

/* START UPDATE */
{&ST_INIT_STATE, EVT_START,
 &ST_SEND_RESET_STATE, act_send_reset},

/* TARGET INFO */
{&ST_SEND_RESET_STATE, EVT_TARGET_INFO,
 &ST_BUILD_PIPE_STATE, act_target_info},

/* BUILD PIPELINE */
{&ST_BUILD_PIPE_STATE, EVT_START,
 &ST_SEND_WINDOW_STATE, act_send_window},

/* SEND DATA WINDOW */
{&ST_SEND_WINDOW_STATE, EVT_SEG_ACK,
 &ST_SEND_WINDOW_STATE, act_seg_ack},

{&ST_SEND_WINDOW_STATE, EVT_SECTOR_END,
 &ST_VERIFY_STATE, act_crc_verify},

/* VERIFY CRC */
{&ST_VERIFY_STATE, EVT_CRC_OK,
 &ST_NEXT_SECTOR_STATE, act_next_sector},

/* NEXT SECTOR */
{&ST_NEXT_SECTOR_STATE, EVT_START,
 &ST_SEND_WINDOW_STATE, act_send_window},

{&ST_NEXT_SECTOR_STATE, EVT_APP_ACK,
 &ST_DONE_STATE, act_done},

};



#ifdef __cplusplus
}
#endif


#endif /* __FSM_TABLE_H__ */