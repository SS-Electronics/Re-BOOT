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
 
/* ====================================================================
   Module constants
   ==================================================================== */
 
/**
 * @brief Width of the ASCII progress bar in characters.
 *
 * Adjust to taste.  20 fits comfortably on an 80-column terminal
 * alongside the percentage and sector information.
 */
#define PROGRESS_BAR_WIDTH  20u

/* ------------------------------------------------------------------ */
/** @brief Static TX packet reused for every outgoing transmission.   */
/* ------------------------------------------------------------------ */
static comm_packet_t tx_pkt;

/**
 * @brief Running count of segments successfully sent this session.
 *
 * Incremented by @ref act_send_window on each successful
 * @c transport_send() call.  Used to compute the progress percentage.
 * Rewound by @ref act_crc_nack when a sector is retransmitted so the
 * bar never shows more than 100%.
 */
static uint32_t segments_sent = 0;
 
/**
 * @brief Total estimated segment count across all sectors.
 *
 * Computed once in @ref act_build_pipeline as:
 * @code
 *   ceil(sector_size / segment_size) * sector_count
 * @endcode
 * Used as the denominator for the progress percentage.
 */
static uint32_t total_segments = 0;

/* ====================================================================
   Internal helpers
   ==================================================================== */
 
/**
 * @brief Render and print the progress bar to stdout in place.
 *
 * Uses @c \\r (carriage return, no newline) so the line is rewritten
 * on every call without scrolling the terminal.  Flushes stdout so
 * the update appears immediately even without a newline.
 *
 * @par Example output
 * @code
 *   Flashing  [=========>          ]  47%  sector 9/19  0x08001200
 * @endcode
 *
 * @param sectors_done  Sectors fully written and verified so far.
 * @param total_sectors Total sectors in the pipeline.
 * @param current_addr  Base flash address of the sector being sent.
 */
static void print_progress(uint32_t sectors_done,
                            uint32_t total_sectors,
                            uint32_t current_addr)
{
    /* Percentage based on segments for fine-grained resolution */
    uint32_t pct = (total_segments > 0u)
                 ? (segments_sent * 100u) / total_segments
                 : 0u;
 
    if (pct > 100u)
        pct = 100u;
 
    /* Build the bar string */
    uint32_t filled = (pct * PROGRESS_BAR_WIDTH) / 100u;
    char     bar[PROGRESS_BAR_WIDTH + 1u];
    uint32_t i;
 
    for (i = 0u; i < PROGRESS_BAR_WIDTH; i++)
    {
        if      (i <  filled) bar[i] = '=';
        else if (i == filled) bar[i] = '>';
        else                  bar[i] = ' ';
    }
    bar[PROGRESS_BAR_WIDTH] = '\0';
 
    /* Overwrite the current terminal line */
    printf("\r  Flashing  [%s]  %3u%%  sector %u/%u  0x%08X   ",
           bar, pct, sectors_done, total_sectors, current_addr);
 
    fflush(stdout);
}

/* ====================================================================
   act_fsm_signal_generation
   ==================================================================== */
 
/**
 * @brief Poll the RX queue and dispatch FSM events.
 *
 * Called from the main loop on every iteration.  Dequeues one packet,
 * maps its @c command byte to the appropriate event, and dispatches it.
 *
 * @par Command → event mapping
 *  - @c RESP_TARGET_INFO   → @c EVT_TARGET_INFO  (packet in event data)
 *  - @c RESP_SEG_ACK       → @c EVT_SEG_ACK
 *  - @c RESP_CRC_ACK       → @c EVT_CRC_OK
 *  - @c RESP_CRC_NACK      → @c EVT_CRC_NACK
 *  - @c RESP_APP_JUMP_ACK  → @c EVT_APP_ACK
 *
 * @param fsm  Pointer to the bootloader FSM instance.
 *
 * @note The dequeued packet is always freed after event dispatch.
 */
void act_fsm_signal_generation(fsm_t *fsm)
{
    void *msg = NULL;
 
    if (!queue_try_pop(&handle_queue_receive_packets, &msg))
        return;
 
    comm_packet_t *pkt = (comm_packet_t *)msg;
 
    switch (pkt->command)
    {
        case RESP_TARGET_INFO:
        {
            fsm_event_t *evt = fsm_event_create(EVT_TARGET_INFO, NULL);
            evt->data = malloc(sizeof(comm_packet_t));
            memcpy(evt->data, pkt, sizeof(comm_packet_t));
            fsm_dispatch(fsm, evt);
            break;
        }
 
        case RESP_SEG_ACK:
            fsm_dispatch(fsm, fsm_event_create(EVT_SEG_ACK, NULL));
            break;
 
        case RESP_CRC_ACK:
            fsm_dispatch(fsm, fsm_event_create(EVT_CRC_OK, NULL));
            break;
 
        case RESP_CRC_NACK:
            fsm_dispatch(fsm, fsm_event_create(EVT_CRC_NACK, NULL));
            break;
 
        case RESP_APP_JUMP_ACK:
            fsm_dispatch(fsm, fsm_event_create(EVT_APP_ACK, NULL));
            break;
 
        default:
            break;
    }
 
    free(pkt);
}
 
 
/* ====================================================================
   act_send_reset
   ==================================================================== */
 
/**
 * @brief Transmit @c CMD_RESET_REQ to the target bootloader.
 *
 * @param e    Triggering event.  Unused.
 * @param fsm  Pointer to the FSM instance.
 */
void act_send_reset(fsm_event_t *e, fsm_t *fsm)
{
    tx_pkt.command  = CMD_RESET_REQ;
    tx_pkt.length   = 1;
    tx_pkt.data[0]  = FLAG_SET;
 
    if (transport_send(&tx_pkt) > 0)
    {
        printf("[ Re-BOOT ] Connecting to target ...\n");
        fileio_printf(&handle_log_file,
                      "[RESET] CMD_RESET_REQ sent\n");
    }
    else
    {
        printf("[ ERR ] Failed to send reset request\n");
        fileio_printf(&handle_log_file,
                      "[ERR ] CMD_RESET_REQ send failed\n");
    }
}
 
 
/* ====================================================================
   act_target_info
   ==================================================================== */
 
/**
 * @brief Parse @c RESP_TARGET_INFO and initialise the bootloader context.
 *
 * Extracts flash start address, sector size, and segment size from
 * the 8-byte payload and stores them in @ref bootloader_ctx_t.
 * Dispatches @c EVT_START to trigger pipeline construction.
 *
 * @par Payload layout
 * @code
 *  Byte 0–3 : Flash start address   (big-endian uint32)
 *  Byte 4–5 : Sector size in bytes  (big-endian uint16)
 *  Byte 6–7 : Segment size in bytes (big-endian uint16)
 * @endcode
 *
 * @param e    Event whose @c data field holds a heap-allocated copy
 *             of the received @ref comm_packet_t.
 * @param fsm  Pointer to the FSM instance.
 */
void act_target_info(fsm_event_t *e, fsm_t *fsm)
{
    comm_packet_t *pkt = (comm_packet_t *)e->data;
 
    if (pkt == NULL)
    {
        printf("[ ERR ] Target info packet is NULL\n");
        fileio_printf(&handle_log_file, "[ERR ] RESP_TARGET_INFO is NULL\n");
        return;
    }
 
    if (pkt->length != 8)
    {
        printf("[ ERR ] Target info length mismatch: got %u, expected 8\n",
               pkt->length);
        fileio_printf(&handle_log_file,
                      "[ERR ] RESP_TARGET_INFO length=%u (expected 8)\n",
                      pkt->length);
        free(pkt);
        return;
    }
 
    uint32_t flash_addr =
        ((uint32_t)pkt->data[0] << 24) |
        ((uint32_t)pkt->data[1] << 16) |
        ((uint32_t)pkt->data[2] <<  8) |
        ((uint32_t)pkt->data[3]);
 
    uint16_t sector_size  = ((uint16_t)pkt->data[4] << 8) | pkt->data[5];
    uint16_t segment_size = ((uint16_t)pkt->data[6] << 8) | pkt->data[7];
 
    /* Console: one clean info block */
    printf("[ Re-BOOT ] Target connected\n");
    printf("            Flash start : 0x%08X\n", flash_addr);
    printf("            Sector size : %u bytes\n", sector_size);
    printf("            Segment size: %u bytes\n\n", segment_size);
 
    /* Log: full detail */
    fileio_printf(&handle_log_file,
                  "[TARGET] flash=0x%08X  sector=%u B  segment=%u B\n",
                  flash_addr, sector_size, segment_size);
 
    bootloader_ctx_t *ctx = CTX(fsm);
    ctx->sector_size   = sector_size;
    ctx->segment_size  = segment_size;
    ctx->retry_count   = 0;
    ctx->max_retries   = MAX_RETRY;
 
    free(pkt);
 
    fsm_dispatch(fsm, fsm_event_create(EVT_START, NULL));
}
 
 
/* ====================================================================
   act_build_pipeline
   ==================================================================== */
 
/**
 * @brief Build the firmware transfer pipeline from parsed HEX records.
 *
 * Calls @c pipeline_build() to create sector-aligned data buffers.
 * Computes @c total_segments so the progress bar has an accurate
 * denominator for the entire session.
 *
 * @param e    Triggering event.  Unused.
 * @param fsm  Pointer to the FSM instance.
 */
void act_build_pipeline(fsm_event_t *e, fsm_t *fsm)
{
    bootloader_ctx_t *ctx = CTX(fsm);
 
    fileio_printf(&handle_log_file,
                  "[PIPELINE] Building: %u records, sector=%u B, segment=%u B\n",
                  ctx->record_count, ctx->sector_size, ctx->segment_size);
 
    pipeline_build(
        &ctx->pipeline,
        ctx->records,
        ctx->record_count,
        ctx->sector_size
    );
 
    /* Compute total segments for progress bar denominator */
    uint32_t segs_per_sector = (ctx->sector_size + ctx->segment_size - 1u)
                             / ctx->segment_size;
 
    total_segments = ctx->pipeline.sector_count * segs_per_sector;
    segments_sent  = 0u;
 
    /* Console: summary before the progress bar begins */
    printf("[ Re-BOOT ] Image ready — %u sectors (~%u segments)\n\n",
           ctx->pipeline.sector_count, total_segments);
 
    fileio_printf(&handle_log_file,
                  "[PIPELINE] Built: %u sectors, ~%u total segments\n",
                  ctx->pipeline.sector_count, total_segments);
 
    ctx->current_sector = 0u;
    ctx->offset         = 0u;
 
    fsm_dispatch(fsm, fsm_event_create(EVT_START, NULL));
}
 
 
/* ====================================================================
   act_send_window
   ==================================================================== */
 
/**
 * @brief Fetch and transmit one firmware segment.
 *
 * Retrieves the next chunk from the current sector via
 * @c pipeline_next_segment() and sends it as @c CMD_PIPELINE_DATA.
 *
 * @par Console output
 * The progress bar is updated in place with @c \\r — no per-segment
 * line is printed so the terminal stays clean.
 *
 * @par Log file output
 * Every segment is recorded with sector index, absolute flash address,
 * byte count, and the first four data bytes for spot-checking.
 *
 * @param e    Triggering event.  Unused.
 * @param fsm  Pointer to the FSM instance.
 */
void act_send_window(fsm_event_t *e, fsm_t *fsm)
{
    bootloader_ctx_t *ctx = CTX(fsm);
 
    uint8_t  seg_data[ctx->segment_size];
    uint32_t seg_addr = 0u;
 
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
        /* Sector exhausted — trigger CRC verify */
        fileio_printf(&handle_log_file,
                      "[SECTOR %u] All segments sent — requesting verify\n",
                      ctx->current_sector);
 
        fsm_dispatch(fsm, fsm_event_create(EVT_SECTOR_END, NULL));
        return;
    }
 
    /* Build and send CMD_PIPELINE_DATA */
    tx_pkt.command  = CMD_PIPELINE_DATA;
    tx_pkt.length   = 4u + (uint16_t)len;
    tx_pkt.data[0]  = (seg_addr >> 24) & 0xFF;
    tx_pkt.data[1]  = (seg_addr >> 16) & 0xFF;
    tx_pkt.data[2]  = (seg_addr >>  8) & 0xFF;
    tx_pkt.data[3]  = (seg_addr >>  0) & 0xFF;
    memcpy(&tx_pkt.data[4], seg_data, (size_t)len);
 
    if (transport_send(&tx_pkt) > 0)
    {
        segments_sent++;
 
        /* Console: update the progress bar in place */
        uint32_t sector_base =
            ctx->pipeline.sectors[ctx->current_sector].base_addr;
 
        print_progress(ctx->current_sector,
                       ctx->pipeline.sector_count,
                       sector_base);
 
        /* Log: full segment record */
        fileio_printf(&handle_log_file,
                    "[SEG TX ] sector=%u  addr=0x%08X  len=%3d B  data=[%02X %02X %02X %02X ...]  total_sent=%u\n",
                    ctx->current_sector, seg_addr, len,
                    (len > 0) ? seg_data[0] : 0u,
                    (len > 1) ? seg_data[1] : 0u,
                    (len > 2) ? seg_data[2] : 0u,
                    (len > 3) ? seg_data[3] : 0u,
                    segments_sent);
    }
    else
    {
        /* Print on a new line so the error does not clobber the bar */
        printf("\n[ ERR ] Segment send failed at 0x%08X\n", seg_addr);
        fileio_printf(&handle_log_file,
                      "[ERR ] SEG TX failed sector=%u addr=0x%08X\n",
                      ctx->current_sector, seg_addr);
    }
}
 
 
/* ====================================================================
   act_seg_ack
   ==================================================================== */
 
/**
 * @brief Handle a segment acknowledgement from the target.
 *
 * The target has buffered the last segment.  Dispatches @c EVT_START
 * to trigger the next segment send.
 *
 * @par Console output
 * None — the progress bar updates on the next @ref act_send_window
 * call.  Printing one ACK line per segment would flood the terminal.
 *
 * @par Log file output
 * One line recording the cumulative segment count at ACK time.
 *
 * @param e    Triggering event.  Unused.
 * @param fsm  Pointer to the FSM instance.
 */
void act_seg_ack(fsm_event_t *e, fsm_t *fsm)
{
    fileio_printf(&handle_log_file,
                  "[SEG ACK] cumulative segments sent: %u\n",
                  segments_sent);
 
    fsm_dispatch(fsm, fsm_event_create(EVT_START, NULL));
}
 
 
/* ====================================================================
   act_crc_verify
   ==================================================================== */
 
/**
 * @brief Send the sector write + CRC verification request.
 *
 * Computes CRC32 of the full sector buffer via @c pipeline_sector_crc()
 * and transmits it in @c CMD_PIPELINE_VERIFY.
 *
 * @par Console output
 * A @c \\n terminates the progress bar line, then a "Verifying …"
 * line is printed so the operator knows a flash write is in progress.
 *
 * @par Log file output
 * Sector index, base address, and CRC32 value.
 *
 * @param e    Triggering event.  Unused.
 * @param fsm  Pointer to the FSM instance.
 */
void act_crc_verify(fsm_event_t *e, fsm_t *fsm)
{
    bootloader_ctx_t *ctx = CTX(fsm);
 
    uint32_t crc = pipeline_sector_crc(&ctx->pipeline, ctx->current_sector);
 
    tx_pkt.command  = CMD_PIPELINE_VERIFY;
    tx_pkt.length   = 4u;
    tx_pkt.data[0]  = (crc >> 24) & 0xFF;
    tx_pkt.data[1]  = (crc >> 16) & 0xFF;
    tx_pkt.data[2]  = (crc >>  8) & 0xFF;
    tx_pkt.data[3]  = (crc >>  0) & 0xFF;
 
    if (transport_send(&tx_pkt) > 0)
    {
        uint32_t sector_base =
            ctx->pipeline.sectors[ctx->current_sector].base_addr;
 
        /* End the progress bar line, then show the verify operation */
        printf("\r  Verifying  sector %2u  0x%08X  CRC32=0x%08X ...%-20s",
            ctx->current_sector, sector_base, crc, "");
        fflush(stdout);
 
        fileio_printf(&handle_log_file,
                      "[VERIFY ] sector=%u  addr=0x%08X  CRC32=0x%08X\n",
                      ctx->current_sector, sector_base, crc);
    }
    else
    {
        printf("\n[ ERR ] Sector %u verify send failed\n",
               ctx->current_sector);
        fileio_printf(&handle_log_file,
                      "[ERR ] VERIFY send failed sector=%u\n",
                      ctx->current_sector);
    }
}
 
 
/* ====================================================================
   act_crc_nack
   ==================================================================== */
 
/**
 * @brief Handle a sector write NACK and retransmit the sector.
 *
 * Increments the retry counter.  If the limit has not been reached,
 * resets the sector offset and segment counter so the progress bar
 * does not over-count retransmitted segments, then dispatches
 * @c EVT_START to retransmit from byte 0.
 *
 * If @c retry_count reaches @c max_retries the FSM is halted.
 *
 * @param e    Triggering event.  Unused.
 * @param fsm  Pointer to the FSM instance.
 */
void act_crc_nack(fsm_event_t *e, fsm_t *fsm)
{
    bootloader_ctx_t *ctx = CTX(fsm);
 
    ctx->retry_count++;
 
    printf("  [ WARN ] Sector %u NACK — retry %u / %u\n",
           ctx->current_sector, ctx->retry_count, ctx->max_retries);
 
    fileio_printf(&handle_log_file,
                  "[NACK   ] sector=%u  retry=%u/%u\n",
                  ctx->current_sector, ctx->retry_count, ctx->max_retries);
 
    if (ctx->retry_count >= ctx->max_retries)
    {
        printf("[ ERR ] Max retries reached on sector %u — aborting\n",
               ctx->current_sector);
        fileio_printf(&handle_log_file,
                      "[ERR ] Sector %u: max retries (%u) exceeded — abort\n",
                      ctx->current_sector, ctx->max_retries);
 
        fsm->fsm_running = FLAG_RESET;
        return;
    }
 
    /* Rewind sector offset so retransmission starts from byte 0 */
    ctx->offset = 0u;
 
    /* Rewind the segment counter so the progress bar stays accurate */
    uint32_t segs_per_sector = (ctx->sector_size + ctx->segment_size - 1u)
                             / ctx->segment_size;
 
    if (segments_sent >= segs_per_sector)
        segments_sent -= segs_per_sector;
    else
        segments_sent = 0u;
 
    fileio_printf(&handle_log_file,
                  "[NACK   ] Retransmitting sector %u from byte 0\n",
                  ctx->current_sector);
 
    fsm_dispatch(fsm, fsm_event_create(EVT_START, NULL));
}
 
 
/* ====================================================================
   act_next_sector
   ==================================================================== */
 
/**
 * @brief Advance to the next sector after a successful write.
 *
 * Resets per-sector state and either begins the next sector or
 * signals that all sectors are done.
 *
 * @par Console output
 * A single "OK" confirmation line showing the verified sector address,
 * then the progress bar immediately resumes for the next sector.
 *
 * @par Log file output
 * Sector written confirmation with address and progress fraction.
 *
 * @param e    Triggering event.  Unused.
 * @param fsm  Pointer to the FSM instance.
 */
void act_next_sector(fsm_event_t *e, fsm_t *fsm)
{
    bootloader_ctx_t *ctx = CTX(fsm);
 
    uint32_t sector_base =
        ctx->pipeline.sectors[ctx->current_sector].base_addr;
 
    printf("\r  [ OK ]  Sector %2u  0x%08X  written and verified\n",
           ctx->current_sector, sector_base,"");
    fflush(stdout);
    
    fileio_printf(&handle_log_file,
                  "[SECTOR ] sector=%u addr=0x%08X  WRITTEN OK  (%u/%u)\n",
                  ctx->current_sector,
                  sector_base,
                  ctx->current_sector + 1u,
                  ctx->pipeline.sector_count);
 
    ctx->retry_count    = 0u;
    ctx->offset         = 0u;
    ctx->current_sector++;
 
    if (ctx->current_sector >= ctx->pipeline.sector_count)
    {
        printf("\n[ Re-BOOT ] All sectors written — starting application ...\n");
        fileio_printf(&handle_log_file,
                      "[PIPELINE] All %u sectors written — sending CMD_START_APP\n",
                      ctx->pipeline.sector_count);
 
        fsm_dispatch(fsm, fsm_event_create(EVT_ALL_SECTORS_DONE, NULL));
    }
    else
    {
        fileio_printf(&handle_log_file,
                      "[PIPELINE] Advancing to sector %u\n",
                      ctx->current_sector);
 
        fsm_dispatch(fsm, fsm_event_create(EVT_START, NULL));
    }
}
 
 
/* ====================================================================
   act_app_jump
   ==================================================================== */
 
/**
 * @brief Send @c CMD_START_APP to the target.
 *
 * Waits for @c RESP_APP_JUMP_ACK before transitioning to @c ST_DONE.
 *
 * @param e    Triggering event.  Unused.
 * @param fsm  Pointer to the FSM instance.
 */
void act_app_jump(fsm_event_t *e, fsm_t *fsm)
{
    tx_pkt.command  = CMD_START_APP;
    tx_pkt.length   = 1u;
    tx_pkt.data[0]  = FLAG_SET;
 
    if (transport_send(&tx_pkt) > 0)
    {
        printf("[ Re-BOOT ] Application start command sent\n");
        fileio_printf(&handle_log_file,
                      "[APP    ] CMD_START_APP sent\n");
    }
    else
    {
        printf("[ ERR ] Application start command failed\n");
        fileio_printf(&handle_log_file,
                      "[ERR   ] CMD_START_APP send failed\n");
    }
}
 
 
/* ====================================================================
   act_done
   ==================================================================== */
 
/**
 * @brief Finalise the firmware update and stop the FSM.
 *
 * Called when @c RESP_APP_JUMP_ACK confirms the target has handed
 * control to the new application.  Forces the progress bar to 100%,
 * prints the completion banner, and clears @c fsm->fsm_running so
 * the main loop exits cleanly.
 *
 * @param e    Triggering event.  Unused.
 * @param fsm  Pointer to the FSM instance.
 */
void act_done(fsm_event_t *e, fsm_t *fsm)
{
    bootloader_ctx_t *ctx = CTX(fsm);
 
    /* Force 100% and redraw the bar one final time */
    segments_sent = total_segments;
    print_progress(ctx->pipeline.sector_count,
                   ctx->pipeline.sector_count,
                   ctx->pipeline.sectors[ctx->pipeline.sector_count - 1u].base_addr);
 
    printf("\n\n");
    printf("  ╔══════════════════════════════════════╗\n");
    printf("  ║   Re-BOOT : Firmware update DONE !   ║\n");
    printf("  ╚══════════════════════════════════════╝\n\n");
 
    fileio_printf(&handle_log_file,
                  "[DONE   ] Firmware upload complete. "
                  "Sectors=%u  Segments=%u\n",
                  ctx->pipeline.sector_count,
                  segments_sent);
 
    fsm->fsm_running = FLAG_RESET;
}