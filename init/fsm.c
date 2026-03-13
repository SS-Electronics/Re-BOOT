/* 
File:        fsm.c
Author:      Subhajit Roy  
             subhajitroy005@gmail.com 

Moudle:      Init  
Info:        Finite State Machine related functions           
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

#include "fsm.h"
#include "transport_layer.h"
#include "bl_protocol_config.h"

#define QSIZE 128


static fsm_entry_t table[]=
{

{ST_INIT, EVT_START,
ST_SEND_RESET,(void*)act_reset},

{ST_SEND_RESET,EVT_TARGET_INFO,
 ST_BUILD_PIPELINE,(void*)act_build_pipeline},

{ST_BUILD_PIPELINE,EVT_START,
 ST_SEND_WINDOW,(void*)act_send_window},

{ST_SEND_WINDOW,EVT_SEG_ACK,
 ST_SEND_WINDOW,(void*)act_seg_ack},

{ST_SEND_WINDOW,EVT_SECTOR_END,
 ST_VERIFY,(void*)act_crc},

{ST_VERIFY,EVT_CRC_OK,
 ST_NEXT_SECTOR,(void*)act_next_sector},

{ST_NEXT_SECTOR,EVT_START,
 ST_SEND_WINDOW,(void*)act_send_window},

{ST_NEXT_SECTOR,EVT_APP_ACK,
 ST_DONE,NULL}

};


static event_msg_t queue[QSIZE];
static int head=0,tail=0;

void fsm_post(event_msg_t *evt)
{
    queue[tail]=*evt;
    tail=(tail+1)%QSIZE;
}

static event_msg_t get_evt()
{
    event_msg_t e=queue[head];
    head=(head+1)%QSIZE;
    return e;
}
static void act_reset()
{
    uint8_t pkt[2]={0xAA,0x55};

    transport_send(pkt,2);

    printf("RESET sent\n");
}

static void act_build_pipeline(event_msg_t *evt)
{
    ctx.sector_size = evt->sector_size;
    ctx.segment_size = evt->segment_size;

    pipeline_build(
        &ctx.pipeline,
        ctx.records,
        ctx.record_count,
        ctx.sector_size);

    ctx.current_sector=0;
    ctx.offset=0;
}

static void act_send_window()
{
    uint8_t data[256];
    uint32_t addr;

    while(ctx.in_flight < WINDOW_SIZE)
    {
        int len = pipeline_next_segment(
            &ctx.pipeline,
            ctx.current_sector,
            &ctx.offset,
            ctx.segment_size,
            &addr,
            data);

        if(len<0)
        {
            event_msg_t e={EVT_SECTOR_END};
            fsm_post(&e);
            break;
        }

        transport_send(data,len);

        ctx.in_flight++;
    }
}

static void act_seg_ack()
{
    if(ctx.in_flight)
        ctx.in_flight--;

    act_send_window();
}

static void act_crc()
{
    uint32_t crc =
        pipeline_sector_crc(
            &ctx.pipeline,
            ctx.current_sector);

    uint8_t pkt[8];

    memcpy(pkt,&crc,4);

    transport_send(pkt,4);
}

static void act_next_sector()
{
    ctx.current_sector++;
    ctx.offset=0;
    ctx.in_flight=0;
}


static void dispatch(event_msg_t *evt)
{
    for(int i=0;i<sizeof(table)/sizeof(table[0]);i++)
    {
        if(table[i].state==ctx.state &&
           table[i].event==evt->type)
        {
            if(table[i].action)
                table[i].action(evt);

            ctx.state = table[i].next;
            break;
        }
    }
}

void fsm_run()
{
    while(ctx.state != ST_DONE)
    {
        event_msg_t evt = get_evt();

        dispatch(&evt);
    }

    printf("Flash finished\n");
}