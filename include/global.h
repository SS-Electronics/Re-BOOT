/* 
File:        global.h
Author:      Subhajit Roy  
             subhajitroy005@gmail.com 

Moudle:      Init  
Info:        Global handles           
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

#ifndef __GLOBAL_H__
#define __GLOBAL_H__


#include "app_types.h"
#include "threads.h"
#include "queues.h"
#include "drv_file_write.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ====================================================================
   FSM State identifiers
   ==================================================================== */
 
/**
 * @brief Identifiers for each state in the firmware update FSM.
 *
 * These values are stored in the @c id field of @ref fsm_state_t and
 * can be used for logging or runtime introspection.  They must match
 * the order in which the state objects are declared in @c fsm_table.c.
 */
typedef enum
{
    /**
     * @brief Idle state before the update procedure starts.
     *
     * The FSM is placed here by @c fsm_init().  Waits for the first
     * @c EVT_START from the application.
     */
    ST_INIT = 0,
 
    /**
     * @brief Reset request sent; waiting for target bootloader response.
     *
     * The host has transmitted @c CMD_RESET_REQ and is waiting for
     * the target to boot its bootloader and reply with
     * @c RESP_TARGET_INFO.
     */
    ST_SEND_RESET,
 
    /**
     * @brief Building the flash-sector pipeline from parsed HEX records.
     *
     * @c pipeline_build() is called here to organise the firmware image
     * into sector-aligned chunks ready for transmission.
     */
    ST_BUILD_PIPELINE,
 
    /**
     * @brief Transmitting firmware segments and awaiting ACKs.
     *
     * One @c CMD_PIPELINE_DATA packet is sent per entry of this state.
     * The FSM loops here, driven by @c EVT_SEG_ACK, until the current
     * sector is exhausted.
     */
    ST_SEND_WINDOW,
 
    /**
     * @brief Sector CRC verification and flash write state.
     *
     * A @c CMD_PIPELINE_VERIFY packet (carrying the sector CRC32) has
     * been sent.  The FSM waits for @c RESP_CRC_ACK or
     * @c RESP_CRC_NACK from the target.
     */
    ST_VERIFY,
 
    /**
     * @brief Sector written successfully; advancing to the next sector.
     *
     * Transfer cursors are updated here.  The FSM either loops back to
     * @c ST_SEND_WINDOW or transitions to @c ST_APP_JUMP when all
     * sectors are done.
     */
    ST_NEXT_SECTOR,
 
    /**
     * @brief Application start command sent; waiting for jump ACK.
     *
     * @c CMD_START_APP has been transmitted.  The FSM waits here for
     * @c RESP_APP_JUMP_ACK before declaring the update complete.
     */
    ST_APP_JUMP,
 
    /**
     * @brief Terminal state — firmware update completed successfully.
     *
     * @c fsm->fsm_running is cleared in this state, causing the main
     * loop to exit.
     */
    ST_DONE
 
} state_id_t;
 
 
/* ====================================================================
   FSM Event identifiers
   ==================================================================== */
 
/**
 * @brief Event identifiers that drive transitions in the firmware
 *        update FSM.
 *
 * Events are created with @c fsm_event_create() and dispatched via
 * @c fsm_dispatch().  The @c act_fsm_signal_generation() function maps
 * incoming protocol response codes to these event IDs.
 */
typedef enum
{
    /**
     * @brief Generic start / proceed trigger.
     *
     * Used in multiple states to kick off the next logical step:
     *  - In @c ST_INIT          : begin the reset sequence.
     *  - In @c ST_BUILD_PIPELINE: begin the first segment send.
     *  - In @c ST_SEND_WINDOW   : send the current (or next) segment.
     *  - In @c ST_NEXT_SECTOR   : begin the next sector's first segment.
     */
    EVT_START = 1,
 
    /**
     * @brief Target responded with bootloader and hardware parameters.
     *
     * Triggered by @c RESP_TARGET_INFO.  The associated event carries
     * a heap-allocated copy of the received @ref comm_packet_t in its
     * @c data field.
     */
    EVT_TARGET_INFO,
 
    /**
     * @brief Target acknowledged receipt of the last segment.
     *
     * Triggered by @c RESP_SEG_ACK.  Causes the FSM to remain in
     * @c ST_SEND_WINDOW and send the next segment.
     */
    EVT_SEG_ACK,
 
    /**
     * @brief All segments of the current sector have been transmitted.
     *
     * Dispatched internally by @c act_send_window() when
     * @c pipeline_next_segment() signals there are no more segments.
     * Transitions the FSM from @c ST_SEND_WINDOW to @c ST_VERIFY.
     */
    EVT_SECTOR_END,
 
    /**
     * @brief Target successfully wrote and verified the current sector.
     *
     * Triggered by @c RESP_CRC_ACK.  Advances the FSM to
     * @c ST_NEXT_SECTOR.
     */
    EVT_CRC_OK,
 
    /**
     * @brief Target rejected the sector write (CRC mismatch or error).
     *
     * Triggered by @c RESP_CRC_NACK.  The @c act_crc_nack() handler
     * resets the sector offset and retransmits the sector, up to the
     * limit defined by @c MAX_RETRY.
     */
    EVT_CRC_NACK,
 
    /**
     * @brief All sectors have been written to flash.
     *
     * Dispatched internally by @c act_next_sector() when
     * @c current_sector reaches @c pipeline.sector_count.
     * Transitions the FSM from @c ST_NEXT_SECTOR to @c ST_APP_JUMP.
     */
    EVT_ALL_SECTORS_DONE,
 
    /**
     * @brief Target confirmed the application jump command.
     *
     * Triggered by @c RESP_APP_JUMP_ACK.  Transitions the FSM from
     * @c ST_APP_JUMP to @c ST_DONE.
     */
    EVT_APP_ACK
 
} event_id_t;
 
 
/* ====================================================================
   Shared global handles
   ==================================================================== */
 
/**
 * @brief Log file write handle shared across all modules.
 *
 * Initialised in @c main.c before the FSM starts.  Every module that
 * needs to write to the log should use this handle with
 * @c fileio_printf().
 */
extern fileio_t handle_log_file;
 
/**
 * @brief Receive packet queue shared between the communication thread
 *        and the FSM signal generator.
 *
 * The @c comm_rx_thread pushes heap-allocated @ref comm_packet_t
 * pointers into this queue.  @c act_fsm_signal_generation() pops
 * and processes them on every main-loop iteration.
 */
extern queue_t handle_queue_receive_packets;


#ifdef __cplusplus
}
#endif

#endif /* __GLOBAL_H__ */