/* 
File:        transport_layer.c
Author:      Subhajit Roy  
             subhajitroy005@gmail.com 

Module:      Comm  
Info:        Switching communication between any communication
             protocol, allowing a generic structure            
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

/* 
File:        transport_layer.c
Author:      Subhajit Roy  
             subhajitroy005@gmail.com 

Module:      Comm  
Info:        Switching communication between any communication
             protocol, allowing a generic structure            
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

/**
 * @file transport_layer.c
 * @brief Driver-independent packet framing for Serial, TCP, UDP, and CAN-over-Serial.
 *
 * @par Supported frame formats
 *
 * @b Serial / TCP / UDP — byte-stream framing with CRC16 integrity check:
 * @code
 *  ┌─────┬─────┬───────┬───────┬──────────────┬───────┬───────┐
 *  │ ':' │ CMD │ LEN_H │ LEN_L │ DATA[0..N-1] │ CRC_H │ CRC_L │
 *  └─────┴─────┴───────┴───────┴──────────────┴───────┴───────┘
 *    1 B   1 B    1 B     1 B       N bytes      1 B     1 B
 * @endcode
 * CRC16-CCITT computed over CMD + LEN_H + LEN_L + DATA.
 *
 * UDP uses the identical frame layout as TCP.  Each call to
 * @c drv_udp_tx() sends one complete framed packet as a single
 * datagram.  @c drv_udp_rx() reads one datagram at a time; the
 * byte-wise state machine then parses the bytes within that
 * datagram in the same way as the Serial and TCP paths.
 *
 * @b CAN-over-Serial — compact framing over the USB-CAN adapter virtual port:
 * @code
 *  ┌─────┬─────┬─────┬──────────────────┐
 *  │ ':' │ CMD │ LEN │ DATA[0..LEN-1]   │
 *  └─────┴─────┴─────┴──────────────────┘
 *    1 B   1 B   1 B      0–8 bytes
 *  Total: 3 + LEN bytes (max 11 bytes)
 * @endcode
 *
 * CAN frame field mapping from @ref comm_packet_t:
 *  - @c pkt->command → CMD   (= CAN ID, low 8 bits used as command code)
 *  - @c pkt->length  → LEN   (single byte, replaces LEN_H:LEN_L, max 8)
 *  - @c pkt->data[]  → DATA  (raw payload, no CRC appended)
 *
 * The @c ':' delimiter is kept for easy re-synchronisation on the wire.
 * No CRC bytes are appended — the CAN hardware provides a 15-bit CRC
 * that the USB-CAN adapter validates before forwarding the frame.
 *
 * @par Why no new CAN driver
 * The USB-CAN adapter exposes itself as a virtual serial port (ttyUSBx).
 * The existing @c drv_serial_tx() / @c drv_serial_rx() calls are reused —
 * only the byte packing inside @c transport_send() and @c transport_receive()
 * differs from the Serial/TCP path.
 */

#include "transport_layer.h"

#include <stdio.h>
#include <string.h>
#include "drv_udp.h"


/* ====================================================================
   Module-level driver handles
   ==================================================================== */

/**
 * @brief Serial driver handle.
 * Used for @c SERIAL and @c CAN modes (CAN adapter = virtual serial port).
 */
static drv_serial_t handle_serial_driver;

/** @brief TCP driver handle.  Used for @c TCP mode only. */
static drv_tcp_t    handle_tcp_driver;

/** @brief UDP driver handle.  Used for @c UDP mode only. */
static drv_udp_t    handle_udp_driver;

/**
 * @brief Active driver type selector.
 *
 * Set to @c SERIAL, @c TCP, @c UDP, or @c CAN by @c transport_init().
 * 0 = not initialised.
 */
static uint32_t driver_type = 0;


/* ====================================================================
   Serial / TCP RX parser state  (module-level — persists across calls)
   ==================================================================== */

/**
 * @brief Current Serial/TCP parser state (0 = idle, waiting for ':').
 *
 * State encoding:
 *  0 — scanning for @c ':' start-of-frame delimiter
 *  1 — reading CMD byte
 *  2 — reading LEN_H byte
 *  3 — reading LEN_L byte
 *  4 — reading DATA bytes
 *  5 — reading CRC_H byte
 *  6 — reading CRC_L byte and validating
 */
static uint8_t  rx_state  = 0;

/** @brief Data byte index within the current Serial/TCP frame payload. */
static uint16_t rx_index  = 0;

/** @brief Payload length decoded from LEN_H:LEN_L (Serial/TCP). */
static uint16_t rx_length = 0;

/** @brief Received CRC16 accumulated across states 5 and 6 (Serial/TCP). */
static uint16_t rx_crc    = 0;


/* ====================================================================
   CAN RX parser state  (module-level — persists across calls)
   ==================================================================== */

/**
 * @brief Current CAN parser state (0 = idle, waiting for ':').
 *
 * State encoding:
 *  0 — scanning for @c ':' start-of-frame delimiter
 *  1 — reading CMD byte         → @c pkt->command
 *  2 — reading LEN byte         → @c pkt->length  (0–8, validated)
 *  3 — reading LEN data bytes   → @c pkt->data[]
 */
static uint8_t can_rx_state = 0;

/** @brief LEN decoded in CAN state 2 — number of data bytes to read. */
static uint8_t can_rx_len   = 0;

/** @brief Data byte index within the current CAN frame payload. */
static uint8_t can_rx_index = 0;


/* ====================================================================
   Internal helpers
   ==================================================================== */

/**
 * @brief Reset the Serial/TCP parser to its idle state.
 *
 * Called on successful packet return, length-overflow error, and CRC
 * failure so the next call always scans cleanly for a new @c ':'.
 */
static void parser_reset(void)
{
    rx_state  = 0;
    rx_index  = 0;
    rx_length = 0;
    rx_crc    = 0;
}

/**
 * @brief Reset the CAN parser to its idle state.
 *
 * Called on successful CAN frame return and on LEN-overflow error so
 * the next call scans cleanly for a new @c ':' delimiter.
 */
static void can_parser_reset(void)
{
    can_rx_state = 0;
    can_rx_len   = 0;
    can_rx_index = 0;
}

/**
 * @brief Compute CRC16-CCITT (polynomial 0x1021, init 0xFFFF).
 *
 * Used only by the Serial and TCP send/receive paths.
 * The CAN path does not call this — integrity is provided by hardware.
 *
 * @param data  Input byte buffer.
 * @param len   Number of bytes to process.
 *
 * @return CRC16-CCITT value.
 */
static uint16_t crc16_ccitt(const uint8_t *data, uint16_t len)
{
    uint16_t crc = 0xFFFF;

    for (uint16_t i = 0; i < len; i++)
    {
        crc ^= (uint16_t)data[i] << 8;

        for (uint8_t j = 0; j < 8u; j++)
        {
            if (crc & 0x8000u)
                crc = (crc << 1) ^ 0x1021u;
            else
                crc <<= 1;
        }
    }

    return crc;
}


/* ====================================================================
   transport_init
   ==================================================================== */

/**
 * @brief Initialise the transport layer and open the active driver.
 *
 * Interface selection via @c cmds->interface string:
 *  - @c "serial" — opens @c cmds->ip as a serial port at 115200 baud.
 *  - @c "tcp"    — connects TCP to @c cmds->ip : @c cmds->port.
 *  - @c "udp"    — opens a UDP socket aimed at @c cmds->ip : @c cmds->port.
 *                  Uses the same frame format as TCP (CRC16-CCITT appended).
 *  - @c "can"    — opens @c cmds->ip as a serial port at 115200 baud.
 *                  Uses the same @c drv_serial driver as "serial" because
 *                  the USB-CAN adapter appears as a virtual COM port.
 *                  Only the frame packing differs.
 *
 * @param cmds  Parsed command-line arguments.
 *
 * @return 0 on success, -1 on failure.
 */
int transport_init(cmd_args_t *cmds)
{
    if (cmds->interface == NULL)
    {
        printf("[ ERR ] Communication interface not provided!\n");
        printf("Use: '-c serial', '-c tcp', '-c udp', or '-c can'\n");
        return -1;
    }

    if (strcmp(cmds->interface, "serial") == 0)
    {
        if (cmds->ip == NULL)
        {
            printf("[ ERR ] Serial port not specified.\n");
            printf("Use: '-i ttyACM0' or '-i COM15'\n");
            return -1;
        }

        driver_type = SERIAL;
        return drv_serial_open(&handle_serial_driver, cmds->ip, 115200);
    }
    else if (strcmp(cmds->interface, "tcp") == 0)
    {
        if (cmds->ip == NULL)
        {
            printf("[ ERR ] TCP IP/port not specified.\n");
            printf("Use: '-i 192.168.0.1 -p 5000'\n");
            return -1;
        }

        driver_type = TCP;
        return drv_tcp_open(&handle_tcp_driver, cmds->ip, cmds->port);
    }
    else if (strcmp(cmds->interface, "udp") == 0)
    {
        if (cmds->ip == NULL)
        {
            printf("[ ERR ] UDP IP/port not specified.\n");
            printf("Use: '-i 192.168.0.1 -p 5000'\n");
            return -1;
        }

        driver_type = UDP;
        return drv_udp_open(&handle_udp_driver, cmds->ip, cmds->port);
    }
    else if (strcmp(cmds->interface, "can") == 0)
    {
        if (cmds->ip == NULL)
        {
            printf("[ ERR ] CAN adapter port not specified.\n");
            printf("Use: '-i ttyUSB0'  (the USB-CAN adapter virtual port)\n");
            return -1;
        }

        /* CAN reuses the serial driver — adapter appears as a virtual COM port.
           Frame packing/parsing is different from the serial path. */
        driver_type = CAN;
        return drv_serial_open(&handle_serial_driver, cmds->ip, 115200);
    }
    else
    {
        printf("[ ERR ] Unknown interface '%s'.\n", cmds->interface);
        printf("Use: '-c serial', '-c tcp', '-c udp', or '-c can'\n");
        return -1;
    }
}


/* ====================================================================
   transport_close
   ==================================================================== */

/**
 * @brief Close the active transport driver and release resources.
 *
 * Both @c SERIAL and @c CAN modes use @c drv_serial, so both call
 * @c drv_serial_close().
 *
 * @param cmds  Command-line arguments (driver type identified via
 *              module-level @c driver_type, @p cmds is unused here).
 */
void transport_close(cmd_args_t *cmds)
{
    if (driver_type == SERIAL || driver_type == CAN)
        drv_serial_close(&handle_serial_driver);
    else if (driver_type == TCP)
        drv_tcp_close(&handle_tcp_driver);
    else if (driver_type == UDP)
        drv_udp_close(&handle_udp_driver);

    driver_type = 0;
}

/**
 * @brief Return the active transport type.
 * @return One of @c SERIAL, @c TCP, @c UDP, @c CAN (from app_config.h).
 */
uint32_t transport_get_type(void)
{
    return driver_type;
}

/* ====================================================================
   transport_send
   ==================================================================== */

/**
 * @brief Encode and transmit one packet through the active transport.
 *
 * @par Serial / TCP / UDP path
 * @code
 *  ':' | CMD(1B) | LEN_H(1B) | LEN_L(1B) | DATA(NB) | CRC_H(1B) | CRC_L(1B)
 * @endcode
 * CRC16-CCITT appended over CMD + LEN_H + LEN_L + DATA.
 * For UDP, the complete framed packet is delivered as a single datagram.
 *
 * @par CAN path
 * @code
 *  ':' | CMD(1B) | LEN(1B) | DATA[0..LEN-1]
 * @endcode
 *  - @b CMD : @c pkt->command  (maps to the 11-bit CAN ID, low 8 bits)
 *  - @b LEN : @c pkt->length   (single byte, 0–8, no LEN_H byte)
 *  - @b DATA: @c pkt->data[]   (raw payload, no CRC bytes appended)
 *
 * @param pkt  Packet to transmit.
 *
 * @return Bytes written (> 0) on success, -1 on error.
 */
int transport_send(comm_packet_t *pkt)
{
    if (!pkt)
        return -1;

    /* ----------------------------------------------------------------
       CAN-over-Serial framing
       ':' | CMD(1B) | LEN(1B) | DATA[0..LEN-1]
       Total: 3 + LEN bytes  (max 11 bytes for LEN=8)
    ---------------------------------------------------------------- */
    if (driver_type == CAN)
    {
        if (pkt->length > CAN_MAX_PAYLOAD)
        {
            printf("[ ERR ] CAN payload %u > CAN_MAX_PAYLOAD (%u)"
                   " — truncating\n", pkt->length, CAN_MAX_PAYLOAD);
        }

        uint8_t len = (pkt->length <= CAN_MAX_PAYLOAD)
                    ? (uint8_t)pkt->length
                    : (uint8_t)CAN_MAX_PAYLOAD;

        /* Build the frame in a local buffer */
        uint8_t frame[3u + CAN_MAX_PAYLOAD];
        uint8_t pos = 0u;

        frame[pos++] = ':';          /* start delimiter — same as Serial  */
        frame[pos++] = pkt->command; /* CMD = CAN ID (low 8 bits)         */
        frame[pos++] = len;          /* LEN = single byte (0–8)           */

        if (len > 0u)
        {
            memcpy(&frame[pos], pkt->data, len);
            pos += len;
        }

        /* No CRC — CAN hardware handles frame integrity */
        return drv_serial_tx(&handle_serial_driver, frame, pos);
    }

    /* ----------------------------------------------------------------
       Serial / TCP framing (byte-stream with CRC16)
       ':' | CMD | LEN_H | LEN_L | DATA | CRC_H | CRC_L
    ---------------------------------------------------------------- */
    if (pkt->length > 2048u - 6u)
        return -1;

    uint8_t  frame[2048];
    uint16_t pos = 0;

    frame[pos++] = ':';
    frame[pos++] = pkt->command;
    frame[pos++] = (pkt->length >> 8) & 0xFF;   /* LEN_H */
    frame[pos++] =  pkt->length        & 0xFF;   /* LEN_L */

    if (pkt->length > 0u)
    {
        memcpy(&frame[pos], pkt->data, pkt->length);
        pos += pkt->length;
    }

    /* CRC16-CCITT over CMD + LEN_H + LEN_L + DATA */
    uint16_t crc = crc16_ccitt(&frame[1], (uint16_t)(pkt->length + 3u));

    frame[pos++] = (crc >> 8) & 0xFF;
    frame[pos++] =  crc        & 0xFF;

    if (driver_type == SERIAL)
        return drv_serial_tx(&handle_serial_driver, frame, pos);

    if (driver_type == TCP)
        return drv_tcp_tx(&handle_tcp_driver, frame, pos);

    if (driver_type == UDP)
        return drv_udp_tx(&handle_udp_driver, frame, pos);

    return -1;
}


/* ====================================================================
   transport_receive
   ==================================================================== */

/**
 * @brief Receive one protocol packet from the active transport.
 *
 * Spins until a complete valid packet arrives or the thread stop flag
 * is cleared.
 *
 * @par Serial / TCP / UDP parser  (7 states)
 * Reconstructs packets from the byte stream and validates CRC16.
 * For UDP each @c drv_udp_rx() call returns one complete datagram;
 * the byte-wise parser then processes the bytes within that datagram
 * in the same way as the Serial and TCP paths.
 * State is preserved in module-level variables across calls.
 *
 * @par CAN parser  (4 states)
 * Parses the compact CAN frame:
 * @code
 *  State 0 : scan for ':' start delimiter  (re-sync on noise/misalign)
 *  State 1 : read CMD byte  → pkt->command
 *  State 2 : read LEN byte  → pkt->length  (validated ≤ CAN_MAX_PAYLOAD)
 *  State 3 : read LEN data bytes → pkt->data[]
 * @endcode
 * No CRC check — the USB-CAN adapter only forwards frames that already
 * passed the CAN hardware CRC before being sent over USB.
 *
 * @param pkt                  Output: populated on success.
 * @param thread_running_flag  Loop exits when this becomes 0.
 *
 * @return Payload byte count (>= 0) on success.
 * @retval -1  NULL pointer or thread stopped.
 * @retval -3  Payload exceeds buffer (Serial/TCP) or CAN_MAX_PAYLOAD (CAN).
 * @retval -5  CRC16 mismatch (Serial/TCP only).
 */
int transport_receive(comm_packet_t *pkt, int32_t *thread_running_flag)
{
    if (!pkt || !thread_running_flag)
        return -1;

    uint8_t byte;

    while (*thread_running_flag)
    {
        /* Read one byte from the active driver */
        int n = -1;

        if (driver_type == SERIAL || driver_type == CAN)
            n = drv_serial_rx(&handle_serial_driver, &byte, 1);
        else if (driver_type == TCP)
            n = drv_tcp_rx(&handle_tcp_driver, &byte, 1);
        else if (driver_type == UDP)
            n = drv_udp_rx(&handle_udp_driver, &byte, 1);

        if (n <= 0)
            continue;

        /* ============================================================
           CAN-over-Serial parser
           Frame: ':' | CMD(1B) | LEN(1B) | DATA[0..LEN-1]
           ============================================================ */
        if (driver_type == CAN)
        {
            switch (can_rx_state)
            {
                /* --------------------------------------------------------
                   State 0: scan for ':' start-of-frame delimiter.
                   Discarding non-':' bytes provides the same self-healing
                   re-sync as the Serial/TCP parser.
                -------------------------------------------------------- */
                case 0:
                    if (byte == (uint8_t)':')
                        can_rx_state = 1;
                    break;

                /* --------------------------------------------------------
                   State 1: CMD byte.
                   Carries the CAN command code (= CAN ID low 8 bits).
                -------------------------------------------------------- */
                case 1:
                    pkt->command = byte;
                    can_rx_state = 2;
                    break;

                /* --------------------------------------------------------
                   State 2: LEN byte — single byte replaces LEN_H:LEN_L.
                   Validate against CAN_MAX_PAYLOAD (8).
                -------------------------------------------------------- */
                case 2:
                    if (byte > CAN_MAX_PAYLOAD)
                    {
                        printf("[ RX CAN ] LEN %u > CAN_MAX_PAYLOAD (%u)"
                               " — discarding frame\n",
                               byte, CAN_MAX_PAYLOAD);
                        can_parser_reset();
                        return -3;
                    }

                    can_rx_len   = byte;
                    pkt->length  = byte;
                    can_rx_index = 0u;

                    /* Zero-length CAN frame — valid, no data phase needed */
                    if (can_rx_len == 0u)
                    {
                        can_parser_reset();
                        return 0;
                    }

                    can_rx_state = 3;
                    break;

                /* --------------------------------------------------------
                   State 3: read exactly LEN data bytes.
                   Return as soon as the last byte is stored.
                -------------------------------------------------------- */
                case 3:
                    pkt->data[can_rx_index++] = byte;

                    if (can_rx_index == can_rx_len)
                    {
                        uint8_t pkt_len = can_rx_len;
                        can_parser_reset();
                        return (int)pkt_len;
                    }
                    break;

                default:
                    can_parser_reset();
                    break;
            }

            continue;   /* back to top of while loop */
        }

        /* ============================================================
           Serial / TCP parser
           Frame: ':' | CMD | LEN_H | LEN_L | DATA | CRC_H | CRC_L
           ============================================================ */
        switch (rx_state)
        {
            /* ----------------------------------------------------------
               State 0: scan for ':' start-of-frame delimiter.
            ---------------------------------------------------------- */
            case 0:
                if (byte == (uint8_t)':')
                    rx_state = 1;
                break;

            /* ----------------------------------------------------------
               State 1: CMD byte.
            ---------------------------------------------------------- */
            case 1:
                pkt->command = byte;
                rx_state = 2;
                break;

            /* ----------------------------------------------------------
               State 2: LEN_H — high byte of payload length.
            ---------------------------------------------------------- */
            case 2:
                rx_length = (uint16_t)((uint16_t)byte << 8);
                rx_state  = 3;
                break;

            /* ----------------------------------------------------------
               State 3: LEN_L — low byte of payload length.
               Skip data phase for zero-length packets.
            ---------------------------------------------------------- */
            case 3:
                rx_length |= (uint16_t)byte;

                if (rx_length > (uint16_t)sizeof(pkt->data))
                {
                    printf("[ RX ] Frame error: length %u exceeds buffer"
                           " %u — discarding\n",
                           rx_length, (uint16_t)sizeof(pkt->data));
                    parser_reset();
                    return -3;
                }

                pkt->length = rx_length;
                rx_index    = 0;
                rx_state    = (rx_length == 0u) ? 5u : 4u;
                break;

            /* ----------------------------------------------------------
               State 4: accumulate data bytes.
            ---------------------------------------------------------- */
            case 4:
                pkt->data[rx_index++] = byte;

                if (rx_index == rx_length)
                    rx_state = 5;
                break;

            /* ----------------------------------------------------------
               State 5: CRC_H byte.
            ---------------------------------------------------------- */
            case 5:
                rx_crc   = (uint16_t)((uint16_t)byte << 8);
                rx_state = 6;
                break;

            /* ----------------------------------------------------------
               State 6: CRC_L byte — validate and return.
               Save rx_crc to a local BEFORE parser_reset() zeroes it.
            ---------------------------------------------------------- */
            case 6:
            {
                rx_crc |= (uint16_t)byte;

                uint16_t received_crc = rx_crc;   /* save before reset */

                uint8_t  crc_buf[COMM_MAX_DATA + 3u];
                uint16_t crc_pos = 0u;

                crc_buf[crc_pos++] = pkt->command;
                crc_buf[crc_pos++] = (pkt->length >> 8) & 0xFF;
                crc_buf[crc_pos++] =  pkt->length        & 0xFF;

                if (pkt->length > 0u)
                {
                    memcpy(&crc_buf[crc_pos], pkt->data, pkt->length);
                    crc_pos += pkt->length;
                }

                uint16_t crc_calc = crc16_ccitt(crc_buf, crc_pos);
                uint16_t pkt_len  = pkt->length;

                parser_reset();   /* all needed values are in locals now */

                if (crc_calc != received_crc)
                {
                    printf("[ RX ] CRC mismatch: calc=0x%04X  rx=0x%04X"
                           "  cmd=0x%02X — discarding\n",
                           crc_calc, received_crc, pkt->command);
                    return -5;
                }

                return (int)pkt_len;
            }

            default:
                parser_reset();
                break;
        }
    }

    return -1;
}


/* ====================================================================
   transport_flush
   ==================================================================== */

/**
 * @brief Flush stale bytes from the receive buffer.
 *
 * Drains the hardware FIFO (or UDP socket receive buffer) until the
 * driver returns 0.  Resets both the Serial/TCP/UDP parser and the
 * CAN parser unconditionally so the next call to
 * @c transport_receive() always starts from a clean state.
 *
 * @return 0 always.
 */
int transport_flush(void)
{
    uint8_t tmp;

    if (driver_type == SERIAL || driver_type == CAN)
    {
        while (drv_serial_rx(&handle_serial_driver, &tmp, 1) > 0)
        {
            /* discard stale bytes */
        }
    }
    else if (driver_type == TCP)
    {
        while (drv_tcp_rx(&handle_tcp_driver, &tmp, 1) > 0)
        {
            /* discard stale bytes */
        }
    }
    else if (driver_type == UDP)
    {
        while (drv_udp_rx(&handle_udp_driver, &tmp, 1) > 0)
        {
            /* discard stale datagrams */
        }
    }

    parser_reset();
    can_parser_reset();

    return 0;
}