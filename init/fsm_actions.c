

#include "fsm_actions.h"

void act_send_reset(fsm_event_t *e, fsm_t *fsm)
{
    printf("HOST -> RESET TARGET\n");

    /* send reset packet */
    transport_send_reset();

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

    pipeline_build();

    fsm_dispatch(fsm, fsm_event_create(EVT_START, NULL));
}

void act_send_window(fsm_event_t *e, fsm_t *fsm)
{
    segment_t *seg = pipeline_next_segment();

    printf("Sending segment\n");

    transport_send_segment(seg);
}

void act_seg_ack(fsm_event_t *e, fsm_t *fsm)
{
    printf("Segment ACK received\n");

    pipeline_mark_segment_complete();

    if (pipeline_sector_complete())
    {
        fsm_dispatch(fsm, fsm_event_create(EVT_SECTOR_END, NULL));
    }
    else
    {
        fsm_dispatch(fsm, fsm_event_create(EVT_START, NULL));
    }
}

void act_crc_verify(fsm_event_t *e, fsm_t *fsm)
{
    printf("Sending CRC for verification\n");

    uint32_t crc = pipeline_sector_crc();

    transport_send_crc(crc);
}

void act_next_sector(fsm_event_t *e, fsm_t *fsm)
{
    printf("Moving to next sector\n");

    if (pipeline_finished())
    {
        transport_finalize();
    }
    else
    {
        pipeline_advance_sector();
        fsm_dispatch(fsm, fsm_event_create(EVT_START, NULL));
    }
}

void act_done(fsm_event_t *e, fsm_t *fsm)
{
    printf("Firmware update DONE\n");

    transport_reboot_target();
}