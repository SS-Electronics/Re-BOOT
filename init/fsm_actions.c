/* 
File:        fsm_actions.c
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

#include "fsm_actions.h"

/**
 * @file fsm_actions.c
 * @brief FSM action handler implementations for the Re-BOOT host application.
 *
 * This module implements all action functions that are executed during
 * state transitions in the firmware update FSM.  Each function maps to
 * a specific step in the following protocol sequence:
 *
 * @par Protocol sequence
 * @code
 *  1.  HOST  →  CMD_RESET_REQ        →  TARGET
 *  2.  HOST  ←  RESP_TARGET_INFO     ←  TARGET  (sector size, segment size)
 *  3.  HOST     builds pipeline
 *  4.  HOST  →  CMD_PIPELINE_DATA    →  TARGET  (one segment)
 *  5.  HOST  ←  RESP_SEG_ACK         ←  TARGET
 *      (repeat 4–5 for every segment in the sector)
 *  6.  HOST  →  CMD_PIPELINE_VERIFY  →  TARGET  (CRC32 of sector)
 *  7a. HOST  ←  RESP_CRC_ACK         ←  TARGET  (sector written OK)
 *  7b. HOST  ←  RESP_CRC_NACK        ←  TARGET  (write failed → retry)
 *      (repeat 4–7 for every sector)
 *  8.  HOST  →  CMD_START_APP        →  TARGET
 *  9.  HOST  ←  RESP_APP_JUMP_ACK    ←  TARGET
 * @endcode
 *
 * All state-specific data is stored in a @ref bootloader_ctx_t instance
 * that lives in @c main.c and is accessible through @c fsm->user_data.
 * Use the @c CTX(fsm) macro defined in @c fsm_actions.h for clean access.
 */
 

/* ------------------------------------------------------------------ */
/** @brief Static TX packet reused for every outgoing transmission.   */
/* ------------------------------------------------------------------ */
static comm_packet_t tx_pkt;


/* ====================================================================
   act_fsm_signal_generation
   ==================================================================== */
 
/**
 * @brief Poll the RX queue and convert packets to FSM events.
 *
 * This function is called from the main loop on every iteration.
 * It attempts to dequeue one packet from @c handle_queue_receive_packets.
 * When a packet is found its @c command field is matched against the
 * known response codes defined in @c bl_protocol_config.h, and the
 * corresponding FSM event is dispatched via @c fsm_dispatch().
 *
 * The mapping is:
 *  - @c RESP_TARGET_INFO   → @c EVT_TARGET_INFO  (packet forwarded as event data)
 *  - @c RESP_SEG_ACK       → @c EVT_SEG_ACK
 *  - @c RESP_CRC_ACK       → @c EVT_CRC_OK
 *  - @c RESP_CRC_NACK      → @c EVT_CRC_NACK
 *  - @c RESP_APP_JUMP_ACK  → @c EVT_APP_ACK
 *
 * @param fsm  Pointer to the bootloader FSM instance.
 *
 * @note For @c RESP_TARGET_INFO the packet is copied into a fresh heap
 *       allocation and attached to the event's @c data pointer so that
 *       @ref act_target_info can read it.  For all other responses the
 *       original packet is freed immediately after dispatch.
 */
void act_fsm_signal_generation(fsm_t *fsm)
{
    void *msg = NULL;
 
    /* Nothing in the queue this cycle — return immediately */
    if (!queue_try_pop(&handle_queue_receive_packets, &msg))
    {
        return;
    }
 
    comm_packet_t *pkt = (comm_packet_t *)msg;
 
    switch (pkt->command)
    {
        case RESP_TARGET_INFO:
        {
            /* Forward the packet payload to act_target_info via event data.
               A separate heap copy is made so the original queue allocation
               can be freed unconditionally below. */
            fsm_event_t *evt = fsm_event_create(EVT_TARGET_INFO, NULL);
 
            evt->data = malloc(sizeof(comm_packet_t));
            memcpy(evt->data, pkt, sizeof(comm_packet_t));
 
            fsm_dispatch(fsm, evt);
            break;
        }
 
        case RESP_SEG_ACK:
            /* Segment received and buffered by the target */
            fsm_dispatch(fsm, fsm_event_create(EVT_SEG_ACK, NULL));
            break;
 
        case RESP_CRC_ACK:
            /* Target successfully wrote and verified the sector */
            fsm_dispatch(fsm, fsm_event_create(EVT_CRC_OK, NULL));
            break;
 
        case RESP_CRC_NACK:
            /* Target rejected the sector (CRC mismatch or write error) */
            fsm_dispatch(fsm, fsm_event_create(EVT_CRC_NACK, NULL));
            break;
 
        case RESP_APP_JUMP_ACK:
            /* Target confirmed the application start command */
            fsm_dispatch(fsm, fsm_event_create(EVT_APP_ACK, NULL));
            break;
 
        default:
            /* Unknown or unsupported response — discard silently */
            break;
    }
 
    /* Always release the original queue packet after processing */
    free(pkt);
}


/* ====================================================================
   act_send_reset
   ==================================================================== */
 
/**
 * @brief Transmit a reset request to the target bootloader.
 *
 * Builds a @c CMD_RESET_REQ packet with a single data byte set to
 * @c FLAG_SET and sends it through the active transport layer.
 *
 * After this call the FSM waits in @c ST_SEND_RESET for the target to
 * boot its bootloader and respond with @c RESP_TARGET_INFO.
 *
 * @param e    FSM event that triggered this transition.  Unused here
 *             but required by the @ref fsm_action_t signature.
 * @param fsm  Pointer to the bootloader FSM instance.
 */
void act_send_reset(fsm_event_t *e, fsm_t *fsm)
{
    /* Populate the reset request packet */
    tx_pkt.command  = CMD_RESET_REQ;
    tx_pkt.length   = 1;
    tx_pkt.data[0]  = FLAG_SET;
 
    if (transport_send(&tx_pkt) > 0)
    {
        printf("[ HOST -> TARGET ] Reset Request sent\n");
        fileio_printf(&handle_log_file, "[ HOST -> TARGET ] Reset Request sent\n");
    }
    else
    {
        printf("[ ERR ] [ HOST -> TARGET ] Reset Request send failed\n");
        fileio_printf(&handle_log_file,
                      "[ ERR ] [ HOST -> TARGET ] Reset Request send failed\n");
    }
}


/* ====================================================================
   act_target_info
   ==================================================================== */
 
/**
 * @brief Parse a RESP_TARGET_INFO packet and initialise the context.
 *
 * The target info payload is exactly 8 bytes with the following layout:
 * @code
 *  Byte  0–3 : Flash start address (big-endian uint32)
 *  Byte  4–5 : Sector size in bytes (big-endian uint16)
 *  Byte  6–7 : Segment size in bytes (big-endian uint16)
 * @endcode
 *
 * The extracted values are written into @ref bootloader_ctx_t so that
 * all subsequent action functions can access the target's parameters
 * without re-parsing the packet.
 *
 * @c EVT_START is dispatched at the end of a successful parse to
 * trigger the transition to @c ST_BUILD_PIPELINE.
 *
 * @param e    FSM event whose @c data field points to a heap-allocated
 *             @ref comm_packet_t containing the RESP_TARGET_INFO payload.
 *             This function frees that allocation before returning.
 * @param fsm  Pointer to the bootloader FSM instance.
 *
 * @retval void  On payload length mismatch or NULL packet the function
 *               logs the error and returns without dispatching any event,
 *               leaving the FSM stalled in @c ST_SEND_RESET.
 */
void act_target_info(fsm_event_t *e, fsm_t *fsm)
{
    comm_packet_t *pkt = (comm_packet_t *)e->data;
 
    /* Guard against a NULL payload — should not happen under normal
       operation but protects against queue corruption */
    if (pkt == NULL)
    {
        printf("[ ERR ] Target info packet is NULL\n");
        fileio_printf(&handle_log_file, "[ ERR ] Target info packet is NULL\n");
        return;
    }
 
    /* Validate the expected payload length (4 addr + 2 sector + 2 segment) */
    if (pkt->length != 8)
    {
        printf("[ ERR ] Target info length mismatch: got %u, expected 8\n",
               pkt->length);
        fileio_printf(&handle_log_file,
                      "[ ERR ] Target info length mismatch: got %u expected 8\n",
                      pkt->length);
 
        free(pkt);
        return;
    }
 
    /* ---- Unpack big-endian fields ---------------------------------- */
 
    /** Flash start address — informational only at this stage */
    uint32_t flash_addr =
        ((uint32_t)pkt->data[0] << 24) |
        ((uint32_t)pkt->data[1] << 16) |
        ((uint32_t)pkt->data[2] <<  8) |
        ((uint32_t)pkt->data[3]);
 
    /** Sector size: how many bytes the target writes per erase+program */
    uint16_t sector_size  = ((uint16_t)pkt->data[4] << 8) | pkt->data[5];
 
    /** Segment size: maximum payload per CMD_PIPELINE_DATA packet */
    uint16_t segment_size = ((uint16_t)pkt->data[6] << 8) | pkt->data[7];
 
    /* ---- Log the received parameters ------------------------------- */
    printf("[ TARGET -> HOST ] Target Info received\n");
    printf("[ TARGET ] Flash start address : 0x%08X\n",  flash_addr);
    printf("[ TARGET ] Sector size         : %u bytes\n", sector_size);
    printf("[ TARGET ] Segment size        : %u bytes\n", segment_size);
 
    fileio_printf(&handle_log_file,
                  "[ TARGET -> HOST ] Target Info received\n");
    fileio_printf(&handle_log_file,
                  "[ TARGET ] Flash=0x%08X  sector=%u  segment=%u\n",
                  flash_addr, sector_size, segment_size);
 
    /* ---- Persist parameters in context for all subsequent stages --- */
    bootloader_ctx_t *ctx = CTX(fsm);
    ctx->sector_size   = sector_size;
    ctx->segment_size  = segment_size;
    ctx->retry_count   = 0;
    ctx->max_retries   = MAX_RETRY;
 
    /* Release the heap copy made in act_fsm_signal_generation */
    free(pkt);
 
    /* Move to pipeline construction */
    fsm_dispatch(fsm, fsm_event_create(EVT_START, NULL));
}


/* ====================================================================
   act_build_pipeline
   ==================================================================== */
 
/**
 * @brief Organise the parsed HEX records into flash sectors.
 *
 * Calls @c pipeline_build() with the full HEX record array and the
 * sector size received from the target.  After this call the pipeline
 * holds a sorted array of @ref sector_pipeline_t entries, each
 * containing the data bytes and a validity bitmap for one sector of
 * flash memory.
 *
 * The sector index and byte offset in the context are both reset to 0
 * so that transmission starts from the very first sector.
 *
 * @c EVT_START is dispatched to move the FSM into @c ST_SEND_WINDOW
 * and begin the first segment transmission.
 *
 * @param e    FSM event that triggered this transition.  Unused.
 * @param fsm  Pointer to the bootloader FSM instance.
 */
void act_build_pipeline(fsm_event_t *e, fsm_t *fsm)
{
    bootloader_ctx_t *ctx = CTX(fsm);
 
    printf("[ PIPELINE ] Building pipeline: %u records, sector=%u bytes\n",
           ctx->record_count, ctx->sector_size);
    fileio_printf(&handle_log_file,
                  "[ PIPELINE ] Building pipeline: %u records, sector=%u bytes\n",
                  ctx->record_count, ctx->sector_size);
 
    /* Organise HEX records into sector-aligned chunks ready for transfer */
    pipeline_build(
        &ctx->pipeline,
        ctx->records,
        ctx->record_count,
        ctx->sector_size
    );
 
    printf("[ PIPELINE ] Total sectors to transfer: %u\n",
           ctx->pipeline.sector_count);
    fileio_printf(&handle_log_file,
                  "[ PIPELINE ] Total sectors: %u\n",
                  ctx->pipeline.sector_count);
 
    /* Reset transmission cursors before the first sector */
    ctx->current_sector = 0;
    ctx->offset         = 0;
 
    /* Trigger the first segment send */
    fsm_dispatch(fsm, fsm_event_create(EVT_START, NULL));
}
 


/* ====================================================================
   act_send_window
   ==================================================================== */
 
/**
 * @brief Fetch and transmit one segment of the current sector.
 *
 * Calls @c pipeline_next_segment() to extract the next chunk of
 * firmware bytes from the active sector, starting at @c ctx->offset.
 * The function advances @c offset internally, so successive calls
 * automatically progress through the sector without any external
 * bookkeeping.
 *
 * @par Packet format (CMD_PIPELINE_DATA payload)
 * @code
 *  Byte 0–3 : Absolute flash address of this segment (big-endian)
 *  Byte 4–N : Segment data bytes (up to ctx->segment_size bytes)
 * @endcode
 *
 * If @c pipeline_next_segment() returns -1 the sector is exhausted.
 * In that case @c EVT_SECTOR_END is dispatched to transition the FSM
 * to @c ST_VERIFY, which sends the CRC and requests a sector write.
 *
 * @param e    FSM event that triggered this transition.  Unused.
 * @param fsm  Pointer to the bootloader FSM instance.
 */
void act_send_window(fsm_event_t *e, fsm_t *fsm)
{
    bootloader_ctx_t *ctx = CTX(fsm);
 
    /* VLA sized to the target-reported segment size */
    uint8_t  seg_data[ctx->segment_size];
    uint32_t seg_addr = 0;
 
    /* Ask the pipeline for the next chunk; returns byte count or -1 */
    int len = pipeline_next_segment(
        &ctx->pipeline,
        ctx->current_sector,
        &ctx->offset,
        ctx->segment_size,
        &seg_addr,
        seg_data
    );
 
    if (len < 0)
    {
        /* No more valid bytes in this sector — request flash write */
        printf("[ PIPELINE ] Sector %u: all segments sent, requesting verify\n",
               ctx->current_sector);
        fileio_printf(&handle_log_file,
                      "[ PIPELINE ] Sector %u: all segments sent\n",
                      ctx->current_sector);
 
        fsm_dispatch(fsm, fsm_event_create(EVT_SECTOR_END, NULL));
        return;
    }
 
    /* ---- Build CMD_PIPELINE_DATA packet ----------------------------- */
 
    tx_pkt.command  = CMD_PIPELINE_DATA;
    tx_pkt.length   = 4 + (uint16_t)len;   /* 4-byte address + data */
 
    /* Pack destination address in big-endian order */
    tx_pkt.data[0]  = (seg_addr >> 24) & 0xFF;
    tx_pkt.data[1]  = (seg_addr >> 16) & 0xFF;
    tx_pkt.data[2]  = (seg_addr >>  8) & 0xFF;
    tx_pkt.data[3]  = (seg_addr >>  0) & 0xFF;
 
    /* Append the data bytes immediately after the address */
    memcpy(&tx_pkt.data[4], seg_data, (size_t)len);
 
    if (transport_send(&tx_pkt) > 0)
    {
        printf("[ HOST -> TARGET ] Segment addr=0x%08X  len=%d bytes\n",
               seg_addr, len);
        fileio_printf(&handle_log_file,
                      "[ HOST -> TARGET ] Segment addr=0x%08X len=%d\n",
                      seg_addr, len);
    }
    else
    {
        printf("[ ERR ] [ HOST -> TARGET ] Segment send failed at 0x%08X\n",
               seg_addr);
        fileio_printf(&handle_log_file,
                      "[ ERR ] Segment send failed at 0x%08X\n", seg_addr);
    }
 
    /* FSM now waits in ST_SEND_WINDOW for RESP_SEG_ACK before continuing */
}


/* ====================================================================
   act_seg_ack
   ==================================================================== */
 
/**
 * @brief Handle a segment acknowledgement from the target.
 *
 * The target has buffered the last segment successfully.  Dispatch
 * @c EVT_START to re-enter @ref act_send_window and transmit the
 * next segment (or discover that the sector is complete).
 *
 * @param e    FSM event that triggered this transition.  Unused.
 * @param fsm  Pointer to the bootloader FSM instance.
 */
void act_seg_ack(fsm_event_t *e, fsm_t *fsm)
{
    printf("[ TARGET -> HOST ] Segment ACK — sending next segment\n");
    fileio_printf(&handle_log_file,
                  "[ TARGET -> HOST ] Segment ACK\n");
 
    /* Continue with the next segment in the current sector */
    fsm_dispatch(fsm, fsm_event_create(EVT_START, NULL));
}


/* ====================================================================
   act_crc_verify
   ==================================================================== */
 
/**
 * @brief Send a sector write + CRC verification request.
 *
 * This action is triggered after all segments of the current sector
 * have been transmitted and acknowledged.  It computes the CRC32 of
 * the complete sector using @c pipeline_sector_crc() and sends it to
 * the target inside a @c CMD_PIPELINE_VERIFY packet.
 *
 * Upon receiving this command the target:
 *  1. Programs the buffered data into flash.
 *  2. Reads back the written sector.
 *  3. Computes its own CRC32 and compares it to the received value.
 *  4. Replies with @c RESP_CRC_ACK on success or @c RESP_CRC_NACK
 *     on any mismatch or programming error.
 *
 * @par CMD_PIPELINE_VERIFY payload layout
 * @code
 *  Byte 0–3 : CRC32 of the sector data (big-endian)
 * @endcode
 *
 * @param e    FSM event that triggered this transition.  Unused.
 * @param fsm  Pointer to the bootloader FSM instance.
 */
void act_crc_verify(fsm_event_t *e, fsm_t *fsm)
{
    bootloader_ctx_t *ctx = CTX(fsm);
 
    /* Compute CRC32 over the complete sector data in the pipeline */
    uint32_t crc = pipeline_sector_crc(&ctx->pipeline, ctx->current_sector);
 
    /* Build the verify packet with the CRC in big-endian order */
    tx_pkt.command  = CMD_PIPELINE_VERIFY;
    tx_pkt.length   = 4;
    tx_pkt.data[0]  = (crc >> 24) & 0xFF;
    tx_pkt.data[1]  = (crc >> 16) & 0xFF;
    tx_pkt.data[2]  = (crc >>  8) & 0xFF;
    tx_pkt.data[3]  = (crc >>  0) & 0xFF;
 
    if (transport_send(&tx_pkt) > 0)
    {
        printf("[ HOST -> TARGET ] Sector %u verify  CRC=0x%08X\n",
               ctx->current_sector, crc);
        fileio_printf(&handle_log_file,
                      "[ HOST -> TARGET ] Sector %u verify CRC=0x%08X\n",
                      ctx->current_sector, crc);
    }
    else
    {
        printf("[ ERR ] Sector verify send failed for sector %u\n",
               ctx->current_sector);
        fileio_printf(&handle_log_file,
                      "[ ERR ] Sector verify send failed sector %u\n",
                      ctx->current_sector);
    }
 
    /* FSM waits in ST_VERIFY for RESP_CRC_ACK or RESP_CRC_NACK */
}

/* ====================================================================
   act_crc_nack
   ==================================================================== */
 
/**
 * @brief Handle a sector write NACK and schedule a retransmission.
 *
 * The target reported that the sector could not be written or the
 * CRC comparison failed.  This function increments @c retry_count
 * and, if the limit has not been exceeded, resets the sector byte
 * offset to 0 and dispatches @c EVT_START so that @ref act_send_window
 * retransmits the entire sector from its first segment.
 *
 * If @c retry_count reaches @c max_retries the FSM is halted by
 * clearing @c fsm->fsm_running, which causes the main loop to exit
 * on the next iteration.
 *
 * @param e    FSM event that triggered this transition.  Unused.
 * @param fsm  Pointer to the bootloader FSM instance.
 */
void act_crc_nack(fsm_event_t *e, fsm_t *fsm)
{
    bootloader_ctx_t *ctx = CTX(fsm);
 
    ctx->retry_count++;
 
    printf("[ TARGET -> HOST ] Sector %u NACK  (attempt %u / %u)\n",
           ctx->current_sector, ctx->retry_count, ctx->max_retries);
    fileio_printf(&handle_log_file,
                  "[ TARGET -> HOST ] Sector %u NACK attempt %u/%u\n",
                  ctx->current_sector, ctx->retry_count, ctx->max_retries);
 
    if (ctx->retry_count >= ctx->max_retries)
    {
        /* Exhausted retries — abort the entire firmware update */
        printf("[ ERR ] Max retries (%u) reached for sector %u. Aborting.\n",
               ctx->max_retries, ctx->current_sector);
        fileio_printf(&handle_log_file,
                      "[ ERR ] Max retries reached sector %u. Aborting.\n",
                      ctx->current_sector);
 
        fsm->fsm_running = FLAG_RESET;
        return;
    }
 
    /* Reset the sector offset so the next EVT_START re-sends from byte 0 */
    ctx->offset = 0;
 
    printf("[ PIPELINE ] Retransmitting sector %u from the beginning\n",
           ctx->current_sector);
    fileio_printf(&handle_log_file,
                  "[ PIPELINE ] Retransmitting sector %u\n",
                  ctx->current_sector);
 
    fsm_dispatch(fsm, fsm_event_create(EVT_START, NULL));
}

/* ====================================================================
   act_next_sector
   ==================================================================== */
 
/**
 * @brief Advance the transfer cursor to the next flash sector.
 *
 * Called after @c RESP_CRC_ACK confirms that the current sector was
 * programmed successfully.  The function:
 *  - Resets @c retry_count to 0 for the new sector.
 *  - Resets @c offset to 0 so transmission starts at the first byte.
 *  - Increments @c current_sector.
 *  - Dispatches @c EVT_ALL_SECTORS_DONE when the last sector is done,
 *    or @c EVT_START to begin the next sector's segment transmission.
 *
 * @param e    FSM event that triggered this transition.  Unused.
 * @param fsm  Pointer to the bootloader FSM instance.
 */
void act_next_sector(fsm_event_t *e, fsm_t *fsm)
{
    bootloader_ctx_t *ctx = CTX(fsm);
 
    /* Sector successfully written — reset per-sector state */
    ctx->retry_count = 0;
    ctx->offset      = 0;
    ctx->current_sector++;
 
    printf("[ PIPELINE ] Sector write OK. Progress: %u / %u sectors done\n",
           ctx->current_sector, ctx->pipeline.sector_count);
    fileio_printf(&handle_log_file,
                  "[ PIPELINE ] Sector OK. Progress %u/%u\n",
                  ctx->current_sector, ctx->pipeline.sector_count);
 
    if (ctx->current_sector >= ctx->pipeline.sector_count)
    {
        /* All sectors have been written — time to start the application */
        printf("[ PIPELINE ] All sectors transferred. Sending app start...\n");
        fileio_printf(&handle_log_file,
                      "[ PIPELINE ] All sectors done. Sending app start.\n");
 
        fsm_dispatch(fsm, fsm_event_create(EVT_ALL_SECTORS_DONE, NULL));
    }
    else
    {
        /* More sectors remain — loop back into ST_SEND_WINDOW */
        fsm_dispatch(fsm, fsm_event_create(EVT_START, NULL));
    }
}

/* ====================================================================
   act_app_jump
   ==================================================================== */
 
/**
 * @brief Send the application start command to the target.
 *
 * After all sectors have been written this function transmits a
 * @c CMD_START_APP packet to instruct the target bootloader to hand
 * off control to the newly programmed application firmware.
 *
 * The FSM then waits in @c ST_APP_JUMP for the target to respond
 * with @c RESP_APP_JUMP_ACK before transitioning to @c ST_DONE.
 *
 * @param e    FSM event that triggered this transition.  Unused.
 * @param fsm  Pointer to the bootloader FSM instance.
 */
void act_app_jump(fsm_event_t *e, fsm_t *fsm)
{
    /* Build the app-start command with a single affirmative data byte */
    tx_pkt.command  = CMD_START_APP;
    tx_pkt.length   = 1;
    tx_pkt.data[0]  = FLAG_SET;
 
    if (transport_send(&tx_pkt) > 0)
    {
        printf("[ HOST -> TARGET ] App start command sent\n");
        fileio_printf(&handle_log_file,
                      "[ HOST -> TARGET ] App start command sent\n");
    }
    else
    {
        printf("[ ERR ] [ HOST -> TARGET ] App start command send failed\n");
        fileio_printf(&handle_log_file,
                      "[ ERR ] App start command send failed\n");
    }
 
    /* FSM waits in ST_APP_JUMP for RESP_APP_JUMP_ACK */
}
 

/* ====================================================================
   act_done
   ==================================================================== */
 
/**
 * @brief Finalise the firmware update process and stop the FSM.
 *
 * Called when @c RESP_APP_JUMP_ACK confirms that the target has
 * successfully started the new application.  Logs the completion
 * message and clears @c fsm->fsm_running, causing the main loop
 * in @c main.c to exit cleanly.
 *
 * @param e    FSM event that triggered this transition.  Unused.
 * @param fsm  Pointer to the bootloader FSM instance.
 */
void act_done(fsm_event_t *e, fsm_t *fsm)
{
    printf("[ DONE ] Firmware upload completed successfully!\n");
    fileio_printf(&handle_log_file,
                  "[ DONE ] Firmware upload completed successfully!\n");
 
    /* Signal the main loop to exit */
    fsm->fsm_running = FLAG_RESET;
}