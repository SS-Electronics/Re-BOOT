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
 * @brief Initialize the transport communication interface.
 *
 * This function selects and initializes the transport driver based on
 * the interface specified in the command arguments structure.
 *
 * Supported interfaces:
 *  - "serial" : Uses serial communication (UART/TTY device)
 *  - "tcp"    : Uses TCP socket communication
 *
 * For serial communication the `ip` field contains the device path
 * (e.g. ttyACM0 or COM15).
 *
 * For TCP communication the `ip` field contains the IP address and
 * the `port` field contains the destination port.
 *
 * @param[in] cmds Pointer to command argument structure containing
 *                 interface type and connection parameters.
 *
 * @return int
 * @retval 0   Initialization successful
 * @retval <0  Initialization failed or invalid parameters
 */
int transport_init(cmd_args_t *cmds)
{
    if(cmds->interface != NULL)
    {
        if (strcmp(cmds->interface, "serial") == 0)
        {
            if(cmds-> ip != NULL)
            {
                driver_type = SERIAL;

                /* Initialize serial driver */
                return drv_serial_open(&handle_serial_driver,
                                    cmds->ip,   /* serial device path */
                                    115200);

                
            }
            else
            {
                printf("[ ERR ] SERIAL port not mentioned!...\n");
                printf("Use: '-i ttyACM0' or '-i COM15'\n");   
            } 
        }
        else if (strcmp(cmds->interface, "tcp") == 0)
        {
            if(cmds-> ip != NULL)
            {
                 driver_type = TCP;

                /* Initialize TCP driver */
                return drv_tcp_open(&handle_tcp_driver,
                                    cmds->ip,
                                    cmds->port);
            }
            else
            {
                printf("[ ERR ] IP or PORT not mentioned!...\n");
                printf("Use: '-i 192.168.0.1 -p 5000'\n");   
            }
        }
        else
        {
            printf("[ ERR ] Communication interface '%s' not valid!\n", cmds->interface);
            printf("Use: 'serial' or 'tcp'\n");

            return -1;
        }
    }
    else
    {
            printf("[ ERR ] Communication interface not provided!\n");
            printf("Use: '-c serial' or '-c tcp'\n");

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

static uint16_t crc16_ccitt(const uint8_t *data, uint16_t len)
{
    uint16_t crc = 0xFFFF;

    for (uint16_t i = 0; i < len; i++)
    {
        crc ^= (uint16_t)data[i] << 8;

        for (uint8_t j = 0; j < 8; j++)
        {
            if (crc & 0x8000)
                crc = (crc << 1) ^ 0x1021;
            else
                crc <<= 1;
        }
    }

    return crc;
}

/**
 * @brief Send packet through active transport in
 * [:][cmd][len_hi][len_lo][data][crc_hi][crc_lo] format
 */
int transport_send(comm_packet_t *pkt)
{
    if (!pkt || pkt->length > 2048 - 6)
        return -1;

    uint8_t frame[2048];
    uint16_t pos = 0;

    /* Start delimiter ':' */
    frame[pos++] = ':';

    /* Command */
    frame[pos++] = pkt->command;

    /* Length */
    frame[pos++] = (pkt->length >> 8) & 0xFF;
    frame[pos++] = pkt->length & 0xFF;

    /* Payload */
    if (pkt->length > 0)
    {
        memcpy(&frame[pos], pkt->data, pkt->length);
        pos += pkt->length;
    }

    /* Calculate CRC over CMD+LEN+DATA */
    uint16_t crc = crc16_ccitt(&frame[1], pkt->length + 3);

    frame[pos++] = (crc >> 8) & 0xFF;
    frame[pos++] = crc & 0xFF;

    /* Send through active driver */
    if (driver_type == SERIAL)
        return drv_serial_tx(&handle_serial_driver, frame, pos);

    if (driver_type == TCP)
        return drv_tcp_tx(&handle_tcp_driver, frame, pos);

    return -1;
}


/**
 * @brief Receive packet from active transport
 * Frame format:
 * [:][cmd][len_hi][len_lo][data][crc_hi][crc_lo]
 */
int transport_receive(comm_packet_t *pkt, int32_t *thread_running_flag)
{
    if (!pkt)
        return -1;

    static uint8_t state = 0;
    static uint16_t index = 0;
    static uint16_t length = 0;
    static uint16_t crc_rx = 0;

    uint8_t byte;

    while (*thread_running_flag)
    {
        int n = -1;

        if (driver_type == SERIAL)
            n = drv_serial_rx(&handle_serial_driver, &byte, 1);
        else if (driver_type == TCP)
            n = drv_tcp_rx(&handle_tcp_driver, &byte, 1);

        if (n <= 0)
            continue;

        switch (state)
        {
        case 0: /* Wait ':' */
            if (byte == ':')
                state = 1;
        break;

        case 1: /* CMD */
            pkt->command = byte;
            state = 2;
        break;

        case 2: /* LEN_H */
            length = ((uint16_t)byte << 8);
            state = 3;
        break;

        case 3: /* LEN_L */
            length |= byte;

            if (length > sizeof(pkt->data))
            {
                state = 0;
                return -3;
            }

            pkt->length = length;
            index = 0;

            state = (length == 0) ? 5 : 4;
        break;

        case 4: /* DATA */
            pkt->data[index++] = byte;

            if (index == length)
                state = 5;
        break;

        case 5: /* CRC_H */
            crc_rx = ((uint16_t)byte << 8);
            state = 6;
        break;

        case 6:
        {
            crc_rx |= byte;

            uint8_t buffer[2048];
            uint16_t pos = 0;

            buffer[pos++] = pkt->command;
            buffer[pos++] = (pkt->length >> 8) & 0xFF;
            buffer[pos++] = pkt->length & 0xFF;

            memcpy(&buffer[pos], pkt->data, pkt->length);
            pos += pkt->length;

            uint16_t crc_calc = crc16_ccitt(buffer, pos);

            uint16_t pkt_len = pkt->length;

            /* reset parser */
            state = 0;
            index = 0;
            length = 0;

            if (crc_calc != crc_rx)
                return -5;

            return pkt_len;
}
        break;
        }
    }

    return -1;
}

int transport_flush(void)
{
    uint8_t tmp;

    while (drv_serial_rx(&handle_serial_driver, &tmp, 1) > 0);
}