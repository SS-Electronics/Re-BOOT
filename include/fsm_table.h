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
along with Re-BOOT. If not, see <https://www.gnu.org/licenses/>.
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


/* ====================================================================
   State object declarations
   ==================================================================== */
 
/**
 * @brief Initial idle state — FSM entry point.
 *
 * @c fsm_init() is called with a pointer to this state so the machine
 * starts here before any event is dispatched.
 */
extern fsm_state_t ST_INIT_STATE;
 
/**
 * @brief Waiting for target bootloader info after CMD_RESET_REQ.
 */
extern fsm_state_t ST_SEND_RESET_STATE;
 
/**
 * @brief Pipeline construction state (visited once per session).
 */
extern fsm_state_t ST_BUILD_PIPE_STATE;
 
/**
 * @brief Segment-by-segment transmission state.
 *
 * The FSM loops in this state, one segment per iteration, waiting
 * for @c RESP_SEG_ACK between each send.
 */
extern fsm_state_t ST_SEND_WINDOW_STATE;
 
/**
 * @brief Sector CRC verification and flash write state.
 */
extern fsm_state_t ST_VERIFY_STATE;
 
/**
 * @brief Sector cursor advancement state.
 *
 * Transitions back to @c ST_SEND_WINDOW_STATE for more sectors or
 * forward to @c ST_APP_JUMP_STATE when all sectors are done.
 */
extern fsm_state_t ST_NEXT_SECTOR_STATE;
 
/**
 * @brief Application start state — waits for @c RESP_APP_JUMP_ACK.
 *
 * Added to model the final handshake after all sectors are written.
 * Prevents the FSM from reaching @c ST_DONE before the target has
 * confirmed the jump.
 */
extern fsm_state_t ST_APP_JUMP_STATE;
 
/**
 * @brief Terminal state — firmware update complete.
 *
 * @c fsm->fsm_running is cleared when this state is entered, causing
 * the main loop to exit.
 */
extern fsm_state_t ST_DONE_STATE;
 
 
/* ====================================================================
   Transition table
   ==================================================================== */
 
/**
 * @brief The firmware update FSM transition table.
 *
 * A flat array of @ref fsm_transition_t entries.  @c fsm_run() scans
 * this array on every dispatched event and executes the first matching
 * row.  @c fsm_table_size must be passed alongside this pointer to
 * @c fsm_init() to bound the search.
 *
 * @see fsm_table.c for the full row-by-row documentation.
 */
extern fsm_transition_t fsm_table[];
 
/**
 * @brief Number of valid entries in @ref fsm_table.
 *
 * Computed at compile time in @c fsm_table.c as:
 * @code
 *   sizeof(fsm_table) / sizeof(fsm_table[0])
 * @endcode
 */
extern const size_t fsm_table_size;

#ifdef __cplusplus
}
#endif

#endif /* __FSM_TABLE_H__ */