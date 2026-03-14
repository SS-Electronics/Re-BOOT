/* 
File:        fsm_actions.h
Author:      Subhajit Roy  
             subhajitroy005@gmail.com 

Moudle:      Init  
Info:        State Based Actions           
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

#ifndef __FSM_ACTIONS_H__
#define __FSM_ACTIONS_H__

#include "app_types.h"
#include "global.h"
#include "bl_protocol_config.h"

#include "fsm.h"
#include "threads.h"
#include "queues.h"

#include "transport_layer.h"

#include "drv_file_write.h"


#ifdef __cplusplus
extern "C" {
#endif

/* Action RX Command polling  */
void act_fsm_signal_generation(fsm_t * fsm);

/* Action function prototypes */

void act_send_reset(fsm_event_t *e, fsm_t *fsm);
void act_target_info(fsm_event_t *e, fsm_t *fsm);
void act_send_window(fsm_event_t *e, fsm_t *fsm);
void act_seg_ack(fsm_event_t *e, fsm_t *fsm);
void act_crc_verify(fsm_event_t *e, fsm_t *fsm);
void act_next_sector(fsm_event_t *e, fsm_t *fsm);
void act_done(fsm_event_t *e, fsm_t *fsm);


#ifdef __cplusplus
}
#endif

#endif /* __FSM_ACTIONS_H__ */