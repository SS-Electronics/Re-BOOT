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
 * @brief FSM action handlers for bootloader host communication.
 *
 * This module implements all action functions used by the firmware
 * update state machine. These actions are responsible for:
 *
 * - Processing packets received from the target
 * - Dispatching FSM events
 * - Sending commands to the target bootloader
 * - Managing firmware transfer pipeline
 *
 * The FSM operates as part of the host-side firmware update system
 * in the Re-BOOT project.
 */

static comm_packet_t transmit_info_packet;


/**
 * @brief Generate FSM signals based on received packets.
 *
 * This function attempts to pop a packet from the receive queue and
 * generates the appropriate FSM event based on the packet command.
 *
 * The function acts as a bridge between the **transport layer**
 * and the **state machine**, converting protocol responses into
 * FSM events.
 *
 * @param fsm
 * Pointer to the state machine instance.
 *
 * @note
 * Packets are expected to be dynamically allocated before being pushed
 * into the queue. The packet memory is released after processing.
 */
void act_fsm_signal_generation(fsm_t *fsm)
{
    void *msg = NULL;

    /* Attempt to pop packet from receive queue */
    if(queue_try_pop(&handle_queue_receive_packets, &msg))
    {
        comm_packet_t *pkt = (comm_packet_t *)msg;

        switch(pkt->command)
        {
            case RESP_TARGET_INFO:
                fsm_dispatch(fsm, fsm_event_create(EVT_TARGET_INFO, pkt));
            break;

            case RESP_SEG_ACK:
                fsm_dispatch(fsm, fsm_event_create(EVT_SEG_ACK, pkt));
            break;

            case RESP_CRC_ACK:
                fsm_dispatch(fsm, fsm_event_create(EVT_CRC_OK, pkt));
            break;

            case RESP_APP_JUMP_ACK:
                fsm_dispatch(fsm, fsm_event_create(EVT_APP_ACK, pkt));
            break;

            default:
            break;
        }

        /* Release packet memory after processing */
        free(pkt);
    }
}


/**
 * @brief Send reset command to target bootloader.
 *
 * This action sends a reset request packet to the target device
 * to initiate communication with the bootloader.
 *
 * After sending the reset command, the system waits for the
 * target device to respond with its bootloader information.
 *
 * @param e
 * FSM event triggering this action.
 *
 * @param fsm
 * Pointer to the FSM instance.
 */
void act_send_reset(fsm_event_t *e, fsm_t *fsm)
{
    printf("[ HOST -> TARGET ] Reset Request sent! ...\n");
    fileio_printf(&handle_log_file,"[ HOST -> TARGET ] Reset Request sent! ...\n");

    /* Build reset request packet */
    transmit_info_packet.command = CMD_RESET_REQ;
    transmit_info_packet.length  = 1;
    transmit_info_packet.data[0] = FLAG_SET;

    /* Send packet to target */
    transport_send(&transmit_info_packet);
}


/**
 * @brief Handle target information response.
 *
 * This action is triggered when the host receives target
 * information from the bootloader.
 *
 * The information typically includes:
 * - Target device identifier
 * - Flash memory parameters
 * - Bootloader capabilities
 *
 * Once received, the firmware update pipeline can be initialized.
 *
 * @param e
 * FSM event containing the received packet.
 *
 * @param fsm
 * Pointer to the FSM instance.
 */
void act_target_info(fsm_event_t *e, fsm_t *fsm)
{
    printf("TARGET -> INFO RECEIVED\n");

    /* Trigger pipeline construction */
    fsm_dispatch(fsm, fsm_event_create(EVT_START, NULL));
}


/**
 * @brief Build firmware transfer pipeline.
 *
 * This action constructs the firmware update pipeline used to
 * transfer firmware data in segments and sectors.
 *
 * Typical tasks include:
 * - Parsing firmware image
 * - Splitting firmware into sectors
 * - Creating transmission segments
 *
 * @param e
 * FSM event triggering this action.
 *
 * @param fsm
 * Pointer to the FSM instance.
 */
void act_build_pipeline(fsm_event_t *e, fsm_t *fsm)
{
    printf("Building firmware pipeline\n");

    /* Build firmware segmentation pipeline */
    // pipeline_build();

    /* Begin firmware transmission */
    fsm_dispatch(fsm, fsm_event_create(EVT_START, NULL));
}


/**
 * @brief Send firmware segment window to target.
 *
 * This action transmits the next firmware segment from the
 * prepared pipeline to the target device.
 *
 * @param e
 * FSM event triggering this action.
 *
 * @param fsm
 * Pointer to the FSM instance.
 */
void act_send_window(fsm_event_t *e, fsm_t *fsm)
{
    printf("Sending segment\n");

    /* Retrieve next segment from pipeline */
    // segment_t *seg = pipeline_next_segment();

    /* Send segment */
    // transport_send_segment(seg);
}


/**
 * @brief Handle segment acknowledgment from target.
 *
 * This action is triggered when the target acknowledges
 * successful reception of a firmware segment.
 *
 * The system marks the segment as complete and determines
 * whether the current sector transmission has finished.
 *
 * @param e
 * FSM event containing the acknowledgment packet.
 *
 * @param fsm
 * Pointer to the FSM instance.
 */
void act_seg_ack(fsm_event_t *e, fsm_t *fsm)
{
    printf("Segment ACK received\n");

    /* Mark segment completion */
    // pipeline_mark_segment_complete();

    /* Move to sector completion stage */
    fsm_dispatch(fsm, fsm_event_create(EVT_SECTOR_END, NULL));
}


/**
 * @brief Send CRC verification request.
 *
 * After all segments of a sector are transmitted, the host
 * sends a CRC checksum to the target for integrity verification.
 *
 * @param e
 * FSM event triggering this action.
 *
 * @param fsm
 * Pointer to the FSM instance.
 */
void act_crc_verify(fsm_event_t *e, fsm_t *fsm)
{
    printf("Sending CRC for verification\n");

    /* Calculate sector CRC */
    // uint32_t crc = pipeline_sector_crc();

    /* Send CRC to target */
    // transport_send_crc(crc);
}


/**
 * @brief Move to the next firmware sector.
 *
 * Once CRC verification is successful, the system advances
 * to the next sector of the firmware image.
 *
 * If all sectors have been transmitted, the update process
 * will finalize.
 *
 * @param e
 * FSM event triggering this action.
 *
 * @param fsm
 * Pointer to the FSM instance.
 */
void act_next_sector(fsm_event_t *e, fsm_t *fsm)
{
    printf("Moving to next sector\n");

    /* Check if firmware transmission finished */
    // if (pipeline_finished())
    // {
    //     transport_finalize();
    // }
    // else
    // {
    //     pipeline_advance_sector();
    //     fsm_dispatch(fsm, fsm_event_create(EVT_START, NULL));
    // }
}


/**
 * @brief Finalize firmware upload process.
 *
 * This action is executed when the firmware update process
 * completes successfully.
 *
 * Responsibilities:
 * - Log completion
 * - Stop the FSM
 * - Optionally reboot the target device
 *
 * @param e
 * FSM event triggering this action.
 *
 * @param fsm
 * Pointer to the FSM instance.
 */
void act_done(fsm_event_t *e, fsm_t *fsm)
{
    printf("[ DONE ] Bootloader Upload completed! ...\n");
    fileio_printf(&handle_log_file,"[ DONE ] Bootloader Upload completed! ...\n");

    /* Stop FSM execution */
    fsm->fsm_running = FLAG_RESET;

    /* Optional target reboot */
    // transport_reboot_target();
}
 