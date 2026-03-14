/* 
File:        transport_layer.c
Author:      Subhajit Roy  
             subhajitroy005@gmail.com 

Moudle:      Comm  
Info:        Switching Communication betwwen any 
             communication protocol, allowing a generic structure            
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

#include "transport_layer.h"

#include <stdio.h>
#include <string.h>

static drv_serial_t handle_serial_driver;
static drv_tcp_t    handle_tcp_driver;

static uint32_t  driver_type = 0;


/**
 * @brief Initialize transport interface
 */
int transport_init(cmd_args_t *cmds)
{
    if (strcmp(cmds->interface, "serial") == 0)
    {
        driver_type = SERIAL;

        /* Initialize serial driver */
        return drv_serial_open(&handle_serial_driver,
                               cmds->ip,   /* serial device path */
                               115200);
    }
    else if (strcmp(cmds->interface, "tcp") == 0)
    {
        driver_type = TCP;

        /* Initialize TCP driver */
        return drv_tcp_open(&handle_tcp_driver,
                            cmds->ip,
                            cmds->port);
    }
    else
    {
        printf("[ERR] Communication interface '%s' not valid!\n", cmds->interface);
        printf("Use: 'serial' or 'tcp'\n");

        return -1;
    }
}


/**
 * @brief Close transport interface
 */
void transport_close(cmd_args_t *cmds)
{
    if (driver_type == SERIAL)
    {
        drv_serial_close(&handle_serial_driver);
    }
    else if (driver_type == TCP)
    {
        drv_tcp_close(&handle_tcp_driver);
    }

    driver_type = 0;
}


/**
 * @brief Send packet through active transport in [:][cmd][len_hi][len_lo][data][#] format
 */
int transport_send(comm_packet_t *pkt)
{
    if (!pkt || pkt->length > 2048 - 5) /* 1(:) +1(cmd)+2(len)+data+1(#) */
        return -1;

    uint8_t frame[2048];
    uint16_t pos = 0;

    /* Start delimiter ':' */
    frame[pos++] = ':';

    /* Command */
    frame[pos++] = (uint8_t)(pkt->command & 0xFF);

    /* Length: 2 bytes, big endian */
    frame[pos++] = (uint8_t)((pkt->length >> 8) & 0xFF);
    frame[pos++] = (uint8_t)(pkt->length & 0xFF);

    /* Payload */
    if (pkt->length > 0)
    {
        memcpy(&frame[pos], pkt->data, pkt->length);
        pos += pkt->length;
    }

    /* End delimiter '#' */
    frame[pos++] = '#';

    /* Send through active driver */
    if (driver_type == SERIAL)
        return drv_serial_tx(&handle_serial_driver, frame, pos);

    if (driver_type == TCP)
        return drv_tcp_tx(&handle_tcp_driver, frame, pos);

    return -1;
}


/**
 * @brief Receive packet from active transport (blocking until full frame)
 *
 * Frame format:
 * [:][cmd][len_hi][len_lo][data...][#]
 */
int transport_receive(comm_packet_t *pkt)
{
    if (!pkt)
        return -1;

    static uint8_t  state = 0;
    static uint16_t index = 0;
    static uint16_t length = 0;

    uint8_t byte;

    while (1)
    {
        int n = -1;

        if (driver_type == SERIAL)
            n = drv_serial_rx(&handle_serial_driver, &byte, 1);
        else if (driver_type == TCP)
            n = drv_tcp_rx(&handle_tcp_driver, &byte, 1);

        if (n <= 0)
            continue; /* wait for next byte */

        switch (state)
        {
            /* Wait for start delimiter ':' */
            case 0:
                if (byte == ':')
                {
                    state = 1;
                }
            break;

            /* Command */
            case 1:
                pkt->command = byte;
                state = 2;
            break;

            /* Length high byte */
            case 2:
                length = ((uint16_t)byte << 8);
                state = 3;
            break;

            /* Length low byte */
            case 3:
                length |= byte;

                if (length > sizeof(pkt->data))
                {
                    state = 0; /* invalid length */
                    return -3;
                }

                pkt->length = length;
                index = 0;

                if (length == 0)
                    state = 5;
                else
                    state = 4;
            break;

            /* Data bytes */
            case 4:
                pkt->data[index++] = byte;

                if (index >= length)
                    state = 5;

             break;

            /* End delimiter '#' */
            case 5:
                if (byte == '#')
                {
                    state = 0;
                    return pkt->length; /* packet complete */
                }
                else
                {
                    state = 0; /* invalid frame */
                    return -4;
                }
            break;
        }
    }
}