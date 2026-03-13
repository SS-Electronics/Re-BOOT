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
along with FreeRTOS-KERNEL. If not, see <https://www.gnu.org/licenses/>.
*/

#include "fsm_actions.h"

void act_send_reset(fsm_event_t *e, fsm_t *fsm)
{
    printf("[ HOST -> TARGET ] Reset Request sent! ...\n");
    fileio_printf(&handle_log_file,"[ HOST -> TARGET ] Reset Request sent! ...\n");

    // /* send reset packet */
    // transport_send_reset();

    fsm_dispatch(fsm, fsm_event_create(EVT_APP_ACK, NULL));

    /* wait for TARGET_INFO event from RX driver */
}

void act_target_info(fsm_event_t *e, fsm_t *fsm)
{
    printf("TARGET -> INFO RECEIVED\n");

    /* build firmware pipeline */

    fsm_dispatch(fsm, fsm_event_create(EVT_START, NULL));
}

void act_build_pipeline(fsm_event_t *e, fsm_t *fsm)
{
    printf("Building firmware pipeline\n");

    // pipeline_build();

    fsm_dispatch(fsm, fsm_event_create(EVT_START, NULL));
}

void act_send_window(fsm_event_t *e, fsm_t *fsm)
{
    // segment_t *seg = pipeline_next_segment();

    printf("Sending segment\n");

    // transport_send_segment(seg);
}

void act_seg_ack(fsm_event_t *e, fsm_t *fsm)
{
    printf("Segment ACK received\n");

    // pipeline_mark_segment_complete();

    // if (pipeline_sector_complete())
    // {
        fsm_dispatch(fsm, fsm_event_create(EVT_SECTOR_END, NULL));
    // }
    // else
    // {
    //     fsm_dispatch(fsm, fsm_event_create(EVT_START, NULL));
    // }
}

void act_crc_verify(fsm_event_t *e, fsm_t *fsm)
{
    printf("Sending CRC for verification\n");

    // uint32_t crc = pipeline_sector_crc();

    // transport_send_crc(crc);
}

void act_next_sector(fsm_event_t *e, fsm_t *fsm)
{
    printf("Moving to next sector\n");

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

void act_done(fsm_event_t *e, fsm_t *fsm)
{
    printf("[ DONE ] Bootloader Upload completed! ...\n");
    fileio_printf(&handle_log_file,"[ DONE ] Bootloader Upload completed! ...\n");

    fsm->fsm_running = FLAG_RESET;
    // transport_reboot_target();
}