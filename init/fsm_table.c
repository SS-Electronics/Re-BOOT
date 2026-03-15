/* 
File:        fsm_table.c
Author:      Subhajit Roy  
             subhajitroy005@gmail.com 

Module:      Init  
Info:        Finite State Machine transition table implementation           
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
 * @file fsm_table.c
 * @brief State object definitions and transition table for the Re-BOOT
 *        firmware update FSM.
 *
 * This file has two responsibilities:
 *
 *  1. @b State objects — one @ref fsm_state_t per logical state with no
 *     entry/exit callbacks (all behaviour lives in the action functions).
 *
 *  2. @b Transition table — a flat array of @ref fsm_transition_t entries
 *     that @c fsm_run() scans linearly on every dispatched event.
 *
 * @par Key design decision — EVT_SEG_ACK wired directly to act_send_window
 *
 * An earlier version of the table routed @c EVT_SEG_ACK through an
 * intermediate @c act_seg_ack action which simply dispatched @c EVT_START,
 * which then triggered @c act_send_window.  This two-hop approach was
 * removed for two reasons:
 *
 *  - It placed two events (@c EVT_SEG_ACK and @c EVT_START) into the FSM
 *    queue simultaneously, creating a brief window where the FSM had
 *    already consumed the ACK but not yet sent the next segment.  A
 *    spurious second ACK arriving in that window could cause a double send.
 *
 *  - It added unnecessary latency for every segment in the transfer.
 *
 * The fix is simply:
 * @code
 *   { &ST_SEND_WINDOW_STATE, EVT_SEG_ACK, &ST_SEND_WINDOW_STATE, act_send_window }
 * @endcode
 * The ACK event now directly calls @c act_send_window, which either
 * sends the next segment or detects that the sector is exhausted and
 * dispatches @c EVT_SECTOR_END.  The transition is atomic from the
 * FSM engine's perspective.
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
 *  ST_SEND_WINDOW ◄─────────────────────────────────────────────┐
 *    │  EVT_START    / act_send_window  (first seg, entry point) │
 *    │  EVT_SEG_ACK  / act_send_window  (next seg, direct wire)  │
 *    │  EVT_SECTOR_END / act_crc_verify                          │
 *    ▼                                                           │
 *  ST_VERIFY                                                     │
 *    │  EVT_CRC_OK   / act_next_sector ────────────────────────►─┘
 *    │  EVT_CRC_NACK / act_crc_nack   (reset offset, retry)──►──┘
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

#include "fsm_table.h"


/* ====================================================================
   State object definitions
   ==================================================================== */

/**
 * @brief Initial idle state — FSM entry point.
 *
 * @c fsm_init() places the machine here.  The only outgoing transition
 * is @c EVT_START, which causes the host to send @c CMD_RESET_REQ.
 */
fsm_state_t ST_INIT_STATE        = { ST_INIT,          NULL, NULL, NULL };

/**
 * @brief Reset request sent; waiting for @c RESP_TARGET_INFO.
 *
 * The host has transmitted @c CMD_RESET_REQ and is blocked waiting for
 * the target bootloader to respond with its flash and communication
 * parameters.
 */
fsm_state_t ST_SEND_RESET_STATE  = { ST_SEND_RESET,    NULL, NULL, NULL };

/**
 * @brief Pipeline construction state — visited exactly once per session.
 *
 * @c pipeline_build() is called here to organise the firmware image
 * into sector-aligned buffers.  The state transitions immediately to
 * @c ST_SEND_WINDOW after the pipeline is ready.
 */
fsm_state_t ST_BUILD_PIPE_STATE  = { ST_BUILD_PIPELINE,NULL, NULL, NULL };

/**
 * @brief Segment transmission state — the FSM spends most of its time here.
 *
 * One @c CMD_PIPELINE_DATA packet is sent per entry.  The state loops
 * on itself driven by @c EVT_SEG_ACK (wired directly to
 * @c act_send_window) until the current sector is exhausted, at which
 * point @c EVT_SECTOR_END transitions to @c ST_VERIFY.
 */
fsm_state_t ST_SEND_WINDOW_STATE = { ST_SEND_WINDOW,   NULL, NULL, NULL };

/**
 * @brief Sector write and CRC verification state.
 *
 * @c CMD_PIPELINE_VERIFY has been sent.  The FSM waits here for either
 * @c RESP_CRC_ACK (sector written OK) or @c RESP_CRC_NACK (failure,
 * retry the sector).
 */
fsm_state_t ST_VERIFY_STATE      = { ST_VERIFY,        NULL, NULL, NULL };

/**
 * @brief Sector cursor advancement state.
 *
 * Entered after a successful @c RESP_CRC_ACK.  Advances the sector
 * index and loops back to @c ST_SEND_WINDOW for the next sector, or
 * forwards to @c ST_APP_JUMP when all sectors are done.
 */
fsm_state_t ST_NEXT_SECTOR_STATE = { ST_NEXT_SECTOR,   NULL, NULL, NULL };

/**
 * @brief Application start state — awaiting @c RESP_APP_JUMP_ACK.
 *
 * @c CMD_START_APP has been transmitted.  The FSM waits here until the
 * target confirms the jump before transitioning to @c ST_DONE.
 */
fsm_state_t ST_APP_JUMP_STATE    = { ST_APP_JUMP,      NULL, NULL, NULL };

/**
 * @brief Terminal state — firmware update complete.
 *
 * @c act_done() clears @c fsm->fsm_running, causing the @c main()
 * loop to exit cleanly.
 */
fsm_state_t ST_DONE_STATE        = { ST_DONE,          NULL, NULL, NULL };


/* ====================================================================
   Transition table
   ==================================================================== */

/**
 * @brief Firmware update FSM transition table.
 *
 * Scanned linearly by @c fsm_run() on every dispatched event.  The
 * first row whose @c from pointer and @c event value match the current
 * state and the event type is executed.
 *
 * @par Reading the table
 * Each row is: @code { &FROM, EVENT, &TO, action_fn } @endcode
 *
 * Rows are grouped by source state, happy-path first, error/retry
 * paths immediately below.
 */
fsm_transition_t fsm_table[] =
{
    /* ------------------------------------------------------------------
       ST_INIT → ST_SEND_RESET
       First event in every session.  Send CMD_RESET_REQ to the target.
    ------------------------------------------------------------------ */
    {
        &ST_INIT_STATE,          /**< From  : initial idle               */
        EVT_START,               /**< Event : session start              */
        &ST_SEND_RESET_STATE,    /**< To    : waiting for target info    */
        act_send_reset           /**< Action: transmit CMD_RESET_REQ     */
    },

    /* ------------------------------------------------------------------
       ST_SEND_RESET → ST_BUILD_PIPELINE
       Target replied with hardware parameters.  Parse and store them,
       then build the pipeline.
    ------------------------------------------------------------------ */
    {
        &ST_SEND_RESET_STATE,    /**< From  : waiting for target info    */
        EVT_TARGET_INFO,         /**< Event : RESP_TARGET_INFO received  */
        &ST_BUILD_PIPE_STATE,    /**< To    : pipeline construction      */
        act_target_info          /**< Action: store sector/segment sizes */
    },

    /* ------------------------------------------------------------------
       ST_BUILD_PIPELINE → ST_SEND_WINDOW
       Pipeline built.  Move to the first segment send immediately.
    ------------------------------------------------------------------ */
    {
        &ST_BUILD_PIPE_STATE,    /**< From  : pipeline construction      */
        EVT_START,               /**< Event : dispatched by act_target_info */
        &ST_SEND_WINDOW_STATE,   /**< To    : segment transmission       */
        act_build_pipeline       /**< Action: call pipeline_build()      */
    },

    /* ------------------------------------------------------------------
       ST_SEND_WINDOW — three outgoing transitions.

       Row 1 — EVT_START:
         Used for the very first segment of each sector (dispatched by
         act_build_pipeline and act_next_sector).  Calls act_send_window
         to transmit one segment, then the FSM waits for EVT_SEG_ACK.

       Row 2 — EVT_SEG_ACK  (KEY FIX):
         Wired DIRECTLY to act_send_window — no intermediate act_seg_ack
         hop.  act_send_window either sends the next segment (loop) or
         detects sector exhaustion and dispatches EVT_SECTOR_END (exit
         to ST_VERIFY).  This eliminates the two-event queue burst of
         the previous design and makes every ACK→send transition atomic.

       Row 3 — EVT_SECTOR_END:
         All segments of the sector were sent and acknowledged.
         Move to verification.
    ------------------------------------------------------------------ */
    {
        &ST_SEND_WINDOW_STATE,   /**< From  : segment transmission       */
        EVT_START,               /**< Event : enter / next sector start  */
        &ST_SEND_WINDOW_STATE,   /**< To    : remain, send first seg     */
        act_send_window          /**< Action: transmit one segment       */
    },
    {
        &ST_SEND_WINDOW_STATE,   /**< From  : segment transmission       */
        EVT_SEG_ACK,             /**< Event : RESP_SEG_ACK received      */
        &ST_SEND_WINDOW_STATE,   /**< To    : remain, send next segment  */
        act_send_window          /**< Action: send next seg (or SECTOR_END) */
    },
    {
        &ST_SEND_WINDOW_STATE,   /**< From  : segment transmission       */
        EVT_SECTOR_END,          /**< Event : sector exhausted           */
        &ST_VERIFY_STATE,        /**< To    : CRC verification           */
        act_crc_verify           /**< Action: send CMD_PIPELINE_VERIFY   */
    },

    /* ------------------------------------------------------------------
       ST_VERIFY — two outgoing transitions.

       Row 1 — EVT_CRC_OK:
         Target wrote and verified the sector.  Advance the sector cursor
         or signal that all sectors are done.

       Row 2 — EVT_CRC_NACK:
         Write failed.  Reset offset back to 0 for the current sector,
         increment the retry counter, and loop back to ST_SEND_WINDOW
         to retransmit every segment of the sector from scratch.
    ------------------------------------------------------------------ */
    {
        &ST_VERIFY_STATE,        /**< From  : CRC verification           */
        EVT_CRC_OK,              /**< Event : RESP_CRC_ACK received      */
        &ST_NEXT_SECTOR_STATE,   /**< To    : advance sector cursor      */
        act_next_sector          /**< Action: bump index or signal done  */
    },
    {
        &ST_VERIFY_STATE,        /**< From  : CRC verification           */
        EVT_CRC_NACK,            /**< Event : RESP_CRC_NACK received     */
        &ST_SEND_WINDOW_STATE,   /**< To    : retransmit sector          */
        act_crc_nack             /**< Action: reset offset, check limit  */
    },

    /* ------------------------------------------------------------------
       ST_NEXT_SECTOR — two outgoing transitions.

       Row 1 — EVT_START:
         More sectors remain.  Loop back and begin sending the next one.

       Row 2 — EVT_ALL_SECTORS_DONE:
         Last sector just finished.  Send the application start command.
    ------------------------------------------------------------------ */
    {
        &ST_NEXT_SECTOR_STATE,   /**< From  : sector advancement         */
        EVT_START,               /**< Event : more sectors remain        */
        &ST_SEND_WINDOW_STATE,   /**< To    : segment transmission       */
        act_send_window          /**< Action: first segment of next sec  */
    },
    {
        &ST_NEXT_SECTOR_STATE,   /**< From  : sector advancement         */
        EVT_ALL_SECTORS_DONE,    /**< Event : all sectors written        */
        &ST_APP_JUMP_STATE,      /**< To    : application start          */
        act_app_jump             /**< Action: transmit CMD_START_APP     */
    },

    /* ------------------------------------------------------------------
       ST_APP_JUMP → ST_DONE
       Target confirmed the application jump.  Log completion and stop.
    ------------------------------------------------------------------ */
    {
        &ST_APP_JUMP_STATE,      /**< From  : application start          */
        EVT_APP_ACK,             /**< Event : RESP_APP_JUMP_ACK received */
        &ST_DONE_STATE,          /**< To    : terminal state             */
        act_done                 /**< Action: log success, stop FSM      */
    },
};

/**
 * @brief Number of entries in @ref fsm_table.
 *
 * Computed at compile time and passed to @c fsm_init() so the engine
 * knows the table bounds without a sentinel value.
 */
const size_t fsm_table_size = sizeof(fsm_table) / sizeof(fsm_table[0]);