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

static bl_ctx_t         ctx;


static const fsm_transition_t table[] =
{

{ST_CONNECT, EV_CONNECT_OK, ST_SEND_RESET},

{ST_SEND_RESET, EV_ACK, ST_WAIT_PIPE_INFO},

{ST_WAIT_PIPE_INFO, EV_PIPE_INFO, ST_VERIFY_HEX_ADDR},

{ST_VERIFY_HEX_ADDR, EV_ACK, ST_SEND_PIPELINE},

{ST_SEND_PIPELINE, EV_PIPELINE_SENT, ST_WAIT_ACK},

{ST_WAIT_ACK, EV_ACK, ST_WAIT_PIPELINE_CRC},

{ST_WAIT_ACK, EV_NACK, ST_SEND_PIPELINE},

{ST_WAIT_PIPELINE_CRC, EV_PIPELINE_CRC, ST_VERIFY_PIPELINE},

{ST_VERIFY_PIPELINE, EV_CRC_OK, ST_NEXT_PIPELINE},

{ST_NEXT_PIPELINE, EV_NEXT, ST_SEND_PIPELINE},

{ST_NEXT_PIPELINE, EV_START_ACK, ST_START_APP},

{ST_START_APP, EV_START_ACK, ST_FINISHED}

};




static bl_state_t get_next_state(bl_state_t state, bl_event_t ev)
{
    for(int i=0;i<sizeof(table)/sizeof(table[0]);i++)
    {
        if(table[i].state == state &&
           table[i].event == ev)
        {
            return table[i].next;
        }
    }

    return ST_ERROR;
}


static bl_event_t state_connect()
{
    if(transport_connect())
        return EV_CONNECT_OK;

    return EV_FAIL;
}

static bl_event_t state_send_reset()
{
    transport_send_reset();
    return EV_ACK;
}


static bl_event_t state_wait_pipe_info()
{
    uint8_t buf[16];

    int len = transport_receive(buf,16);

    if(len > 0 && buf[0] == RESP_PIPE_INFO)
    {
        ctx.sector_size =
        (buf[1]<<24)|(buf[2]<<16)|(buf[3]<<8)|buf[4];

        ctx.target_addr =
        (buf[5]<<24)|(buf[6]<<16)|(buf[7]<<8)|buf[8];

        return EV_PIPE_INFO;
    }

    return EV_FAIL;
}

static bl_event_t state_verify_addr()
{
    if(ctx.hex_base_addr == ctx.target_addr)
        return EV_ACK;

    return EV_FAIL;
}

static bl_event_t state_send_pipeline()
{
    ctx.pipeline_start = ctx.current_line;

    if(pipeline_send(ctx.records,
                     ctx.total_lines,
                     &ctx.current_line,
                     ctx.sector_size))
    {
        return EV_PIPELINE_SENT;
    }

    return EV_FAIL;
}


static bl_event_t state_wait_ack()
{
    uint8_t resp;

    if(!transport_receive(&resp,1))
        return EV_FAIL;

    if(resp == RESP_ACK)
    {
        ctx.retry = 0;
        return EV_ACK;
    }

    if(resp == RESP_NACK)
    {
        if(ctx.retry++ < MAX_RETRY)
            return EV_NACK;

        return EV_FAIL;
    }

    return EV_FAIL;
}

static bl_event_t state_wait_crc()
{
    uint8_t buf[8];

    if(transport_receive(buf,8))
    {
        if(buf[0] == RESP_PIPELINE_CRC)
            return EV_PIPELINE_CRC;
    }

    return EV_FAIL;
}

static bl_event_t state_verify_crc()
{
    uint32_t crc =
        pipeline_calculate_crc(ctx.records,
                               ctx.pipeline_start,
                               ctx.current_line);

    uint32_t target_crc;

    transport_receive((uint8_t*)&target_crc,4);

    if(crc == target_crc)
        return EV_CRC_OK;

    return EV_FAIL;
}

static bl_event_t state_next_pipeline()
{
    if(ctx.current_line >= ctx.total_lines)
        return EV_START_ACK;

    return EV_NEXT;
}

static bl_event_t state_start_app()
{
    transport_send_start();

    uint8_t resp;

    transport_receive(&resp,1);

    if(resp == RESP_ACK)
        return EV_START_ACK;

    return EV_FAIL;
}

void bootloader_run(hex_record_t *records,
                    int32_t total_lines,
                    uint32_t hex_base_addr,
                    uint32_t hex_end_addr)
{

    ctx.records = records;
    ctx.total_lines = total_lines;
    ctx.hex_base_addr = hex_base_addr;
    ctx.hex_end_addr = hex_end_addr;

    ctx.current_line = 0;
    ctx.state = ST_CONNECT;

    bl_event_t ev;

    while(ctx.state != ST_FINISHED)
    {

        switch(ctx.state)
        {

        case ST_CONNECT:
            ev = state_connect();
        break;

        case ST_SEND_RESET:
            ev = state_send_reset();
        break;

        case ST_WAIT_PIPE_INFO:
            ev = state_wait_pipe_info();
        break;

        case ST_VERIFY_HEX_ADDR:
            ev = state_verify_addr();
        break;

        case ST_SEND_PIPELINE:
            ev = state_send_pipeline();
        break;

        case ST_WAIT_ACK:
            ev = state_wait_ack();
        break;

        case ST_WAIT_PIPELINE_CRC:
            ev = state_wait_crc();
        break;

        case ST_VERIFY_PIPELINE:
            ev = state_verify_crc();
        break;

        case ST_NEXT_PIPELINE:
            ev = state_next_pipeline();
        break;

        case ST_START_APP:
            ev = state_start_app();
        break;

        default:
            ev = EV_FAIL;
        }

        ctx.state = get_next_state(ctx.state, ev);

        if(ctx.state == ST_ERROR)
        {
            printf("Bootloader failed\n");
            return;
        }
    }

    printf("Flash completed successfully\n");
}