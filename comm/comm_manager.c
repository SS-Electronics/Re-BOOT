
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