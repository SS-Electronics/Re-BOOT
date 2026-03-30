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


/* ============================================================
   Bootloader context
   ============================================================ */
 
/**
 * @brief Runtime context shared across all FSM states.
 *
 * This structure is allocated in @c main.c, initialised before
 * the FSM starts, and passed as @c user_data so every action
 * function can reach it via @c CTX(fsm).
 *
 * Members that come from the target (sector_size, segment_size)
 * are filled in by @ref act_target_info once the target responds
 * with its hardware parameters.
 */
typedef struct
{
    /**
     * @brief Pointer to the parsed HEX record array.
     *
     * Points into the memory pool allocated in @c main.c.
     * Must be set before the FSM is started.
     */
    hex_record_t *records;
 
    /**
     * @brief Total number of valid HEX records.
     *
     * Populated from @c hex_file_lines in @c main.c after
     * @c read_hex_file() returns successfully.
     */
    uint32_t record_count;
 
    /**
     * @brief Pipeline builder instance.
     *
     * Owns the dynamically allocated sector array.
     * Initialised by @ref act_build_pipeline via
     * @c pipeline_build().
     */
    pipeline_builder_t pipeline;
 
    /**
     * @brief Flash sector size reported by the target (bytes).
     *
     * Received in the RESP_TARGET_INFO packet and stored
     * by @ref act_target_info.
     */
    uint16_t sector_size;
 
    /**
     * @brief Communication segment size reported by the target (bytes).
     *
     * The maximum payload that fits in one CMD_PIPELINE_DATA
     * packet. Received in the RESP_TARGET_INFO packet and stored
     * by @ref act_target_info.
     */
    uint16_t segment_size;
 
    /**
     * @brief Index of the sector currently being transmitted.
     *
     * Ranges from 0 to @c pipeline.sector_count - 1.
     * Incremented by @ref act_next_sector after a successful
     * sector write ACK from the target.
     */
    uint32_t current_sector;
 
    /**
     * @brief Byte offset within the current sector.
     *
     * Passed to @c pipeline_next_segment() and advanced by
     * that function after each segment is consumed.
     * Reset to 0 when moving to a new sector or retrying.
     */
    uint32_t offset;
 
    /**
     * @brief Number of segment packets currently in flight.
     *
     * Reserved for future sliding-window extension.
     * Currently unused (single-ACK protocol).
     */
    uint32_t in_flight;
 
    /**
     * @brief Current retry count for the active sector.
     *
     * Incremented by @ref act_crc_nack on each NACK from the
     * target. Reset to 0 by @ref act_next_sector after a
     * successful sector write.
     */
    uint32_t retry_count;
 
    /**
     * @brief Maximum allowed retries per sector.
     *
     * Initialised from @c MAX_RETRY in @ref act_target_info.
     * If @c retry_count reaches this value the FSM is halted.
     */
    uint32_t max_retries;
 
    /**
     * @brief Communication thread running flag.
     *
     * Mirrors the flag owned by @c main.c.
     * Reserved for future use.
     */
    uint8_t thread_running;

    /* HEX file address range from parser ---------------- */
    uint32_t            hex_base_address;   /**< Lowest address in HEX file  */
    uint32_t            hex_end_address;    /**< One-past-last address in HEX */

    /**
     * @brief Target node identifier supplied via the @c -n command-line option.
     *
     * Sent as the second byte of @c CMD_RESET_REQ so the target bootloader
     * can verify it is the intended recipient.  The target echoes this value
     * back in @c RESP_TARGET_INFO (byte 8); @ref act_target_info validates
     * the echo and aborts if there is a mismatch.
     */
    int                 node_id;

} bootloader_ctx_t;

/* ============================================================
   Helper macro
   ============================================================ */
 
/**
 * @brief Retrieve the bootloader context from an FSM instance.
 *
 * Casts @c fsm->user_data to @ref bootloader_ctx_t so action
 * functions can access pipeline state without an explicit cast
 * on every access.
 *
 * @param fsm  Pointer to the @ref fsm_t instance.
 */
#define CTX(fsm)  ((bootloader_ctx_t *)((fsm)->user_data))


/* Action RX Command polling  */
void act_fsm_signal_generation(fsm_t * fsm);

/* ============================================================
   Action function declarations
   ============================================================ */
 
/**
 * @brief Send a reset request to the target bootloader.
 *
 * Builds and transmits a @c CMD_RESET_REQ packet.
 * The FSM then waits in @c ST_SEND_RESET until the target
 * replies with @c RESP_TARGET_INFO.
 *
 * @param e    FSM event that triggered this transition (unused).
 * @param fsm  Pointer to the FSM instance.
 */
void act_send_reset(fsm_event_t *e, fsm_t *fsm);
 
/**
 * @brief Parse target information and initialise the context.
 *
 * Called when @c RESP_TARGET_INFO is received.  Extracts
 * flash start address, sector size, and segment size from the
 * packet payload, stores them in @ref bootloader_ctx_t, and
 * dispatches @c EVT_START to trigger pipeline construction.
 *
 * @param e    FSM event carrying the received @c comm_packet_t
 *             pointer in its @c data field.
 * @param fsm  Pointer to the FSM instance.
 *
 * @note The packet pointed to by @c e->data is freed inside
 *       this function.
 */
void act_target_info(fsm_event_t *e, fsm_t *fsm);
 
/**
 * @brief Build the firmware transfer pipeline.
 *
 * Calls @c pipeline_build() with the HEX records and target
 * sector size stored in @ref bootloader_ctx_t, organising the
 * firmware image into a set of flash sectors ready for
 * transmission.
 *
 * After the pipeline is built, @c EVT_START is dispatched to
 * move the FSM into @c ST_SEND_WINDOW.
 *
 * @param e    FSM event that triggered this transition (unused).
 * @param fsm  Pointer to the FSM instance.
 */
void act_build_pipeline(fsm_event_t *e, fsm_t *fsm);
 
/**
 * @brief Transmit one firmware segment to the target.
 *
 * Calls @c pipeline_next_segment() to fetch the next chunk of
 * data from the current sector and sends it as a
 * @c CMD_PIPELINE_DATA packet.
 *
 * If the sector is exhausted (no more segments), @c EVT_SECTOR_END
 * is dispatched to trigger CRC verification.
 *
 * The FSM remains in @c ST_SEND_WINDOW and waits for the
 * target to reply with @c RESP_SEG_ACK before the next segment
 * is sent.
 *
 * @param e    FSM event that triggered this transition (unused).
 * @param fsm  Pointer to the FSM instance.
 */
void act_send_window(fsm_event_t *e, fsm_t *fsm);
 
/**
 * @brief Handle a segment acknowledgement from the target.
 *
 * Called when @c RESP_SEG_ACK is received.  Dispatches
 * @c EVT_START to send the next segment in the sector.
 *
 * @param e    FSM event that triggered this transition (unused).
 * @param fsm  Pointer to the FSM instance.
 */
void act_seg_ack(fsm_event_t *e, fsm_t *fsm);
 
/**
 * @brief Send a sector write and CRC verification request.
 *
 * Called after all segments of the current sector have been
 * acknowledged.  Computes the CRC32 of the sector via
 * @c pipeline_sector_crc() and transmits it as a
 * @c CMD_PIPELINE_VERIFY packet.
 *
 * The target flashes the buffered sector data, validates the
 * CRC, and replies with either @c RESP_CRC_ACK or
 * @c RESP_CRC_NACK.
 *
 * @param e    FSM event that triggered this transition (unused).
 * @param fsm  Pointer to the FSM instance.
 */
void act_crc_verify(fsm_event_t *e, fsm_t *fsm);
 
/**
 * @brief Handle a sector write NACK and initiate a retry.
 *
 * Called when @c RESP_CRC_NACK is received.  Increments the
 * retry counter and, if the limit has not been reached, resets
 * the sector offset to 0 and dispatches @c EVT_START to
 * retransmit the entire sector from the beginning.
 *
 * If @c retry_count reaches @c max_retries the FSM is halted
 * by clearing @c fsm->fsm_running.
 *
 * @param e    FSM event that triggered this transition (unused).
 * @param fsm  Pointer to the FSM instance.
 */
void act_crc_nack(fsm_event_t *e, fsm_t *fsm);
 
/**
 * @brief Advance to the next firmware sector.
 *
 * Called after a successful sector write ACK (@c RESP_CRC_ACK).
 * Resets the retry counter and sector offset, increments
 * @c current_sector, and either dispatches @c EVT_START to
 * begin the next sector or @c EVT_ALL_SECTORS_DONE when all
 * sectors have been written.
 *
 * @param e    FSM event that triggered this transition (unused).
 * @param fsm  Pointer to the FSM instance.
 */
void act_next_sector(fsm_event_t *e, fsm_t *fsm);
 
/**
 * @brief Send the application start command to the target.
 *
 * Called when all sectors have been successfully written.
 * Transmits @c CMD_START_APP and waits for the target to
 * respond with @c RESP_APP_JUMP_ACK.
 *
 * @param e    FSM event that triggered this transition (unused).
 * @param fsm  Pointer to the FSM instance.
 */
void act_app_jump(fsm_event_t *e, fsm_t *fsm);
 
/**
 * @brief Finalise the firmware upload process.
 *
 * Called when @c RESP_APP_JUMP_ACK is received.  Logs the
 * successful completion and clears @c fsm->fsm_running to
 * exit the main loop cleanly.
 *
 * @param e    FSM event that triggered this transition (unused).
 * @param fsm  Pointer to the FSM instance.
 */
void act_done(fsm_event_t *e, fsm_t *fsm);

#ifdef __cplusplus
}
#endif

#endif /* __FSM_ACTIONS_H__ */