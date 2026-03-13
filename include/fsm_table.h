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


/**
 * @file firmware_update_fsm.h
 * @brief Firmware Update Finite State Machine definitions.
 *
 * This file defines the states, events, and state instances used by the
 * firmware update FSM. The FSM controls the process of sending firmware
 * sectors to a target device using a segmented transfer protocol.
 *
 * Workflow overview:
 * 1. Initialize update session
 * 2. Send reset command to target
 * 3. Build sector pipeline
 * 4. Send data window (segmented transfer)
 * 5. Verify CRC
 * 6. Move to next sector
 * 7. Finish update
 */

#ifndef __FSM_TABLE_H__
#define __FSM_TABLE_H__


#include "fsm.h"
#include "fsm_actions.h"


#ifdef __cplusplus
extern "C" {
#endif


/**
 * @brief FSM state object declarations.
 *
 * Each state object represents a state in the FSM.
 * Entry/exit/action callbacks can be attached if required.
 */
extern fsm_state_t ST_INIT_STATE;
extern fsm_state_t ST_SEND_RESET_STATE;
extern fsm_state_t ST_BUILD_PIPE_STATE;
extern fsm_state_t ST_SEND_WINDOW_STATE;
extern fsm_state_t ST_VERIFY_STATE;
extern fsm_state_t ST_NEXT_SECTOR_STATE;
extern fsm_state_t ST_DONE_STATE;


/**
 * @brief Firmware update FSM transition table.
 *
 * Each entry defines:
 * - Current state
 * - Trigger event
 * - Next state
 * - Action function
 *
 * The FSM engine scans this table to determine the next
 * transition during event dispatch.
 */
extern fsm_transition_t fsm_table[];
extern const size_t fsm_table_size;

#ifdef __cplusplus
}
#endif

#endif /* __FSM_TABLE_H__ */