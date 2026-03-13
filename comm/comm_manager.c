
#include "comm_manager.h"


void* tx_thread(void *arg)
{
    while(1)
    {
        packet_t p = queue_pop(&tx_queue);

        transport_send(p.data,p.len);
    }
}

void* rx_thread(void *arg)
{
    uint8_t buf[1024];

    while(1)
    {
        int n = transport_recv(buf,sizeof(buf));

        if(n>0)
        {
            parse_packet(buf,n);
        }
    }
}

void send_window()
{
    while(win.in_flight < win.window_size)
    {
        send_segment();
        win.in_flight++;
    }
}


void boot_rx_handler(packet_t *pkt)
{
    switch(pkt->type)
    {
        case PKT_TARGET_INFO:
            fsm_dispatch(&fsm, fsm_event_create(EVT_TARGET_INFO, pkt));
        break;

        case PKT_SEG_ACK:
            fsm_dispatch(&fsm, fsm_event_create(EVT_SEG_ACK, pkt));
        break;

        case PKT_CRC_OK:
            fsm_dispatch(&fsm, fsm_event_create(EVT_CRC_OK, pkt));
        break;

        case PKT_APP_ACK:
            fsm_dispatch(&fsm, fsm_event_create(EVT_APP_ACK, pkt));
        break;
    }
}