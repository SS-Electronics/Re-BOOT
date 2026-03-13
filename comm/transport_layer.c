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
along with FreeRTOS-KERNEL. If not, see <https://www.gnu.org/licenses/>.
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
 * @brief Send packet through active transport
 */
int transport_send(comm_packet_t *pkt)
{
    uint8_t frame[2048];
    uint16_t pos = 0;

    if (pkt->length > sizeof(frame) - 4)
        return -1;

    /* command */
    memcpy(&frame[pos], &pkt->command, sizeof(pkt->command));
    pos += sizeof(pkt->command);

    /* length */
    memcpy(&frame[pos], &pkt->length, sizeof(pkt->length));
    pos += sizeof(pkt->length);

    /* payload */
    memcpy(&frame[pos], pkt->data, pkt->length);
    pos += pkt->length;

    if (driver_type == SERIAL)
        return drv_serial_tx(&handle_serial_driver, frame, pos);

    if (driver_type == TCP)
        return drv_tcp_tx(&handle_tcp_driver, frame, pos);

    return -1;
}


/**
 * @brief Receive packet from active transport
 */
int transport_receive(comm_packet_t *pkt)
{
    uint8_t buf[2048];

    int n = -1;

    if (driver_type == SERIAL)
        n = drv_serial_rx(&handle_serial_driver, buf, sizeof(buf));

    else if (driver_type == TCP)
        n = drv_tcp_rx(&handle_tcp_driver, buf, sizeof(buf));

    if (n <= 0)
        return -1;

    uint16_t pos = 0;

    /* command */
    memcpy(&pkt->command, &buf[pos], sizeof(pkt->command));
    pos += sizeof(pkt->command);

    /* length */
    memcpy(&pkt->length, &buf[pos], sizeof(pkt->length));
    pos += sizeof(pkt->length);

    if (pkt->length > sizeof(pkt->data))
        return -1;

    /* payload */
    memcpy(pkt->data, &buf[pos], pkt->length);

    return pkt->length;
}