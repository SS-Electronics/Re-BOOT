/* 
File:        fsm_table.c
Author:      Subhajit Roy  
             subhajitroy005@gmail.com 

Moudle:      Init  
Info:        Finite State Machine Chart implementation            
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

#include "fsm_table.h"

/**
 * @file fsm_table.c
 * @brief State instances and transition table for the Re-BOOT firmware
 *        update FSM.
 *
 * This file has two responsibilities:
 *
 *  1. **State objects** — one @ref fsm_state_t per logical state.  No
 *     entry or exit callbacks are wired at this stage; all behaviour
 *     lives in the action functions declared in @c fsm_actions.h.
 *
 *  2. **Transition table** — a flat array of @ref fsm_transition_t
 *     entries that the FSM engine (@c fsm_run()) scans linearly on
 *     every dispatched event.  Each entry encodes:
 *     @code
 *       { source_state, trigger_event, destination_state, action_fn }
 *     @endcode
 *
 * @par Complete state-machine flow
 * @code
 *  ST_INIT
 *    │  EVT_START / act_send_reset
 *    ▼
 *  ST_SEND_RESET
 *    │  EVT_TARGET_INFO / act_target_info
 *    ▼
 *  ST_BUILD_PIPELINE
 *    │  EVT_START / act_build_pipeline
 *    ▼
 *  ST_SEND_WINDOW  ◄────────────────────────────────────┐
 *    │  EVT_START     / act_send_window   (next segment) │ (from ST_NEXT_SECTOR
 *    │  EVT_SEG_ACK   / act_seg_ack       (next segment) │  or NACK retry)
 *    │  EVT_SECTOR_END/ act_crc_verify
 *    ▼
 *  ST_VERIFY
 *    │  EVT_CRC_OK   / act_next_sector
 *    │  EVT_CRC_NACK / act_crc_nack  ──► reset offset ──►(ST_SEND_WINDOW)
 *    ▼
 *  ST_NEXT_SECTOR
 *    │  EVT_START           / act_send_window  (more sectors)
 *    │  EVT_ALL_SECTORS_DONE/ act_app_jump
 *    ▼
 *  ST_APP_JUMP
 *    │  EVT_APP_ACK / act_done
 *    ▼
 *  ST_DONE
 * @endcode
 */
 

/* ====================================================================
   State object definitions
   ==================================================================== */
 
/**
 * @brief Initial idle state before the update procedure begins.
 *
 * The FSM is placed into this state by @c fsm_init().  The only valid
 * event here is @c EVT_START, which causes the host to send a reset
 * request to the target.
 */
fsm_state_t ST_INIT_STATE        = { ST_INIT,          NULL, NULL, NULL };
 
/**
 * @brief Waiting state after the reset request has been sent.
 *
 * The host stays here until the target bootloader has booted and
 * responds with a @c RESP_TARGET_INFO packet carrying the flash and
 * communication parameters.
 */
fsm_state_t ST_SEND_RESET_STATE  = { ST_SEND_RESET,    NULL, NULL, NULL };
 
/**
 * @brief Pipeline construction state.
 *
 * The HEX records are organised into flash-sector-sized chunks in
 * this state.  The state is visited exactly once per firmware update
 * session and always transitions immediately to @c ST_SEND_WINDOW.
 */
fsm_state_t ST_BUILD_PIPE_STATE  = { ST_BUILD_PIPELINE,NULL, NULL, NULL };
 
/**
 * @brief Segment transmission and acknowledgement state.
 *
 * The FSM remains in this state while sending individual segment
 * packets and waiting for @c RESP_SEG_ACK between each one.  The
 * state loops on itself via @c EVT_SEG_ACK until the current sector
 * is exhausted, at which point @c EVT_SECTOR_END transitions to
 * @c ST_VERIFY.
 */
fsm_state_t ST_SEND_WINDOW_STATE = { ST_SEND_WINDOW,   NULL, NULL, NULL };
 
/**
 * @brief Sector write and CRC verification state.
 *
 * After all segments of a sector have been sent, the host transmits
 * the sector's CRC32 checksum.  The target programs the flash sector,
 * reads it back, and responds with either @c RESP_CRC_ACK or
 * @c RESP_CRC_NACK.
 */
fsm_state_t ST_VERIFY_STATE      = { ST_VERIFY,        NULL, NULL, NULL };
 
/**
 * @brief Sector advancement state.
 *
 * Entered after a successful sector write ACK.  The context cursors
 * are updated and the FSM either loops back to @c ST_SEND_WINDOW for
 * the next sector or proceeds to @c ST_APP_JUMP when all sectors are
 * done.
 */
fsm_state_t ST_NEXT_SECTOR_STATE = { ST_NEXT_SECTOR,   NULL, NULL, NULL };
 
/**
 * @brief Application start state.
 *
 * All flash sectors have been written.  The host sends
 * @c CMD_START_APP and waits for @c RESP_APP_JUMP_ACK from the
 * target before declaring the update complete.
 */
fsm_state_t ST_APP_JUMP_STATE    = { ST_APP_JUMP,      NULL, NULL, NULL };
 
/**
 * @brief Terminal state — firmware update completed.
 *
 * Entered after @c RESP_APP_JUMP_ACK is received.  The @c act_done
 * action clears @c fsm->fsm_running, causing the main loop to exit.
 */
fsm_state_t ST_DONE_STATE        = { ST_DONE,          NULL, NULL, NULL };
 
 
/* ====================================================================
   Transition table
   ==================================================================== */
 
/**
 * @brief Flat transition table for the firmware update FSM.
 *
 * The FSM engine (@c fsm_run()) iterates this table on every event.
 * The first entry whose @c from and @c event fields match the current
 * state and the dispatched event is executed: the @c action callback
 * is called, then the FSM advances to the @c to state.
 *
 * @par Reading guide
 * Each row has the form:
 * @code
 *   { &FROM_STATE, EVENT, &TO_STATE, action_function }
 * @endcode
 *
 * Rows are grouped by source state and ordered so that the happy
 * path reads top-to-bottom, with error/retry paths immediately
 * below the normal transition for the same state.
 */
fsm_transition_t fsm_table[] =
{
    /* ------------------------------------------------------------------
       ST_INIT
       EVT_START kicks off the update by sending CMD_RESET_REQ.
    ------------------------------------------------------------------ */
    {
        &ST_INIT_STATE,         /**< From  : initial idle state          */
        EVT_START,              /**< Event : operator-initiated start    */
        &ST_SEND_RESET_STATE,   /**< To    : waiting for target reset    */
        act_send_reset          /**< Action: transmit CMD_RESET_REQ      */
    },
 
    /* ------------------------------------------------------------------
       ST_SEND_RESET
       Target responded with hardware parameters — parse and store them.
    ------------------------------------------------------------------ */
    {
        &ST_SEND_RESET_STATE,   /**< From  : waiting for target info     */
        EVT_TARGET_INFO,        /**< Event : RESP_TARGET_INFO received   */
        &ST_BUILD_PIPE_STATE,   /**< To    : pipeline construction       */
        act_target_info         /**< Action: extract sector/segment sizes*/
    },
 
    /* ------------------------------------------------------------------
       ST_BUILD_PIPELINE
       Pipeline is built; move immediately to the first segment send.
    ------------------------------------------------------------------ */
    {
        &ST_BUILD_PIPE_STATE,   /**< From  : pipeline construction       */
        EVT_START,              /**< Event : dispatched by act_target_info*/
        &ST_SEND_WINDOW_STATE,  /**< To    : segment transmission        */
        act_build_pipeline      /**< Action: call pipeline_build()       */
    },
 
    /* ------------------------------------------------------------------
       ST_SEND_WINDOW — three transitions out of this state:
       1. EVT_START   : send the next segment (initial entry or after ACK)
       2. EVT_SEG_ACK : target ACK'd last segment → send the next one
       3. EVT_SECTOR_END: sector exhausted → send CRC verify request
    ------------------------------------------------------------------ */
    {
        &ST_SEND_WINDOW_STATE,  /**< From  : segment transmission        */
        EVT_START,              /**< Event : dispatched to trigger send  */
        &ST_SEND_WINDOW_STATE,  /**< To    : remain in segment tx state  */
        act_send_window         /**< Action: transmit one segment        */
    },
    {
        &ST_SEND_WINDOW_STATE,  /**< From  : segment transmission        */
        EVT_SEG_ACK,            /**< Event : RESP_SEG_ACK received       */
        &ST_SEND_WINDOW_STATE,  /**< To    : remain in segment tx state  */
        act_seg_ack             /**< Action: dispatch EVT_START for next */
    },
    {
        &ST_SEND_WINDOW_STATE,  /**< From  : segment transmission        */
        EVT_SECTOR_END,         /**< Event : no more segments in sector  */
        &ST_VERIFY_STATE,       /**< To    : sector CRC verification     */
        act_crc_verify          /**< Action: send CMD_PIPELINE_VERIFY    */
    },
 
    /* ------------------------------------------------------------------
       ST_VERIFY — two transitions depending on target response:
       1. EVT_CRC_OK  : sector written successfully → advance sector
       2. EVT_CRC_NACK: write failed → retransmit sector (retry logic)
    ------------------------------------------------------------------ */
    {
        &ST_VERIFY_STATE,       /**< From  : sector CRC verification     */
        EVT_CRC_OK,             /**< Event : RESP_CRC_ACK received       */
        &ST_NEXT_SECTOR_STATE,  /**< To    : sector advancement          */
        act_next_sector         /**< Action: bump cursor or signal done  */
    },
    {
        &ST_VERIFY_STATE,       /**< From  : sector CRC verification     */
        EVT_CRC_NACK,           /**< Event : RESP_CRC_NACK received      */
        &ST_SEND_WINDOW_STATE,  /**< To    : back to segment tx (retry)  */
        act_crc_nack            /**< Action: reset offset, check retries */
    },
 
    /* ------------------------------------------------------------------
       ST_NEXT_SECTOR — two transitions:
       1. EVT_START          : more sectors remain → loop back to send
       2. EVT_ALL_SECTORS_DONE: last sector done → send app-start command
    ------------------------------------------------------------------ */
    {
        &ST_NEXT_SECTOR_STATE,  /**< From  : sector advancement          */
        EVT_START,              /**< Event : dispatched when sectors left */
        &ST_SEND_WINDOW_STATE,  /**< To    : segment transmission        */
        act_send_window         /**< Action: send first segment of next  */
    },
    {
        &ST_NEXT_SECTOR_STATE,  /**< From  : sector advancement          */
        EVT_ALL_SECTORS_DONE,   /**< Event : all sectors written         */
        &ST_APP_JUMP_STATE,     /**< To    : application start           */
        act_app_jump            /**< Action: transmit CMD_START_APP      */
    },
 
    /* ------------------------------------------------------------------
       ST_APP_JUMP
       Target jumped to application and replied with ACK → finish.
    ------------------------------------------------------------------ */
    {
        &ST_APP_JUMP_STATE,     /**< From  : application start           */
        EVT_APP_ACK,            /**< Event : RESP_APP_JUMP_ACK received  */
        &ST_DONE_STATE,         /**< To    : terminal state              */
        act_done                /**< Action: log completion, stop FSM    */
    },
};
 
/**
 * @brief Number of entries in @ref fsm_table.
 *
 * Computed at compile time.  Passed to @c fsm_init() so the engine
 * knows the table bounds without relying on a sentinel value.
 */
const size_t fsm_table_size = sizeof(fsm_table) / sizeof(fsm_table[0]);