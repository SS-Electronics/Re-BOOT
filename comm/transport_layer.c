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
 * @brief Generic transport layer — driver-independent packet framing.
 *
 * This module sits between the FSM/protocol layer and the physical
 * communication drivers (serial or TCP).  It handles:
 *
 *  - Frame construction and transmission via @c transport_send()
 *  - Byte-stream parsing and frame reconstruction via @c transport_receive()
 *  - CRC16-CCITT integrity checking on both TX and RX paths
 *
 * @par Wire frame format
 * @code
 *  ┌────┬─────┬───────┬───────┬──────────────┬───────┬───────┐
 *  │ ':' │ CMD │ LEN_H │ LEN_L │ DATA[0..N-1] │ CRC_H │ CRC_L │
 *  └────┴─────┴───────┴───────┴──────────────┴───────┴───────┘
 *    1B    1B     1B      1B        N bytes       1B      1B
 * @endcode
 *
 * The CRC16-CCITT is computed over: CMD + LEN_H + LEN_L + DATA.
 * The start delimiter @c ':' is NOT included in the CRC.
 *
 * @par Parser design — why module-level state matters
 * @c transport_receive() is called repeatedly from a dedicated RX
 * thread.  Its internal parser must survive across calls — each call
 * may process only a few bytes before returning, so the parser position
 * must be remembered.  The variables are therefore declared at
 * @b module level (file scope) rather than as local statics or locals.
 *
 * Using module-level (non-stack) state also makes it straightforward
 * to call @c parser_reset() from any error path without worrying about
 * C's "local static initialised once" semantics.
 */

#include "transport_layer.h"

#include <stdio.h>
#include <string.h>

/* ====================================================================
   Driver handles and active driver selector
   ==================================================================== */

/** @brief Serial driver handle. Initialised by @c transport_init(). */
static drv_serial_t handle_serial_driver;

/** @brief TCP driver handle. Initialised by @c transport_init(). */
static drv_tcp_t    handle_tcp_driver;

/**
 * @brief Active driver selector.
 *
 * Set to @c SERIAL or @c TCP by @c transport_init() and consulted
 * by @c transport_send(), @c transport_receive(), and
 * @c transport_flush().
 */
static uint32_t driver_type = 0;


/* ====================================================================
   RX parser state  (module-level — persists across calls)
   ==================================================================== */

/**
 * @brief Current parser state (0–6).
 *
 * Encodes which field the parser is currently waiting for:
 *  0 = waiting for start delimiter ':'
 *  1 = reading CMD byte
 *  2 = reading LEN_H byte
 *  3 = reading LEN_L byte
 *  4 = reading DATA bytes
 *  5 = reading CRC_H byte
 *  6 = reading CRC_L byte
 */
static uint8_t  rx_state  = 0;

/**
 * @brief Data byte index within the current packet's payload.
 *
 * Counts bytes received into @c pkt->data[] during state 4.
 * Reset to 0 at the start of each new frame.
 */
static uint16_t rx_index  = 0;

/**
 * @brief Payload length decoded from LEN_H:LEN_L.
 *
 * Set in state 3 and used in state 4 to know when the data phase
 * is complete.  Also used in the CRC verification buffer in state 6.
 */
static uint16_t rx_length = 0;

/**
 * @brief Received CRC16 value accumulated across states 5 and 6.
 *
 * State 5 loads the high byte; state 6 loads the low byte and then
 * performs the validation.
 */
static uint16_t rx_crc    = 0;


/* ====================================================================
   Internal helpers
   ==================================================================== */

/**
 * @brief Reset the RX parser to its initial idle state.
 *
 * Called on every successful packet return, on every framing error,
 * on every CRC failure, and when the payload length exceeds the
 * buffer.  Resetting all four parser variables together ensures the
 * parser always starts a new frame from a clean slate regardless of
 * how the previous one ended.
 */
static void parser_reset(void)
{
    rx_state  = 0;
    rx_index  = 0;
    rx_length = 0;
    rx_crc    = 0;
}

/**
 * @brief Compute CRC16-CCITT (polynomial 0x1021, init 0xFFFF).
 *
 * This is the same algorithm used by the target-side bootloader so
 * that both ends of the link agree on the checksum for every frame.
 *
 * The CRC is computed MSB-first (non-reflected), matching the
 * original CCITT specification.
 *
 * @param data  Pointer to the input byte buffer.
 * @param len   Number of bytes to include in the calculation.
 *
 * @return Computed CRC16-CCITT value.
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
   Public API
   ==================================================================== */

/**
 * @brief Initialise the transport layer and open the active driver.
 *
 * Reads the @c interface field from @p cmds and selects the
 * appropriate driver:
 *  - @c "serial" — opens the serial port at @c cmds->ip with baud
 *                  rate 115200.
 *  - @c "tcp"    — connects to the TCP server at @c cmds->ip :
 *                  @c cmds->port.
 *
 * @param cmds  Pointer to parsed command-line arguments.
 *
 * @return 0 on success, negative on failure.
 */
int transport_init(cmd_args_t *cmds)
{
    if (cmds->interface == NULL)
    {
        printf("[ ERR ] Communication interface not provided!\n");
        printf("Use: '-c serial' or '-c tcp'\n");
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
    else
    {
        printf("[ ERR ] Unknown interface '%s'.\n", cmds->interface);
        printf("Use: 'serial' or 'tcp'\n");
        return -1;
    }
}

/**
 * @brief Close the active transport driver and release resources.
 *
 * @param cmds  Pointer to command-line arguments (used to identify
 *              which driver to close).
 */
void transport_close(cmd_args_t *cmds)
{
    if (driver_type == SERIAL)
        drv_serial_close(&handle_serial_driver);
    else if (driver_type == TCP)
        drv_tcp_close(&handle_tcp_driver);

    driver_type = 0;
}

/**
 * @brief Encode and transmit one packet over the active transport.
 *
 * Builds the complete wire frame in a local buffer, appends the
 * CRC16-CCITT over CMD+LEN+DATA, and hands it to the driver.
 *
 * @par Frame layout
 * @code
 *  ':' | CMD | LEN_H | LEN_L | DATA[0..N-1] | CRC_H | CRC_L
 * @endcode
 *
 * @param pkt  Pointer to the packet to transmit.
 *
 * @return Number of bytes written to the driver (> 0) on success,
 *         or -1 if @p pkt is NULL or the payload is too large.
 */
int transport_send(comm_packet_t *pkt)
{
    if (!pkt || pkt->length > 2048u - 6u)
        return -1;

    uint8_t  frame[2048];
    uint16_t pos = 0;

    /* ---- Build frame ------------------------------------------------ */

    frame[pos++] = ':';                          /* start delimiter      */
    frame[pos++] = pkt->command;                 /* command byte         */
    frame[pos++] = (pkt->length >> 8) & 0xFF;   /* payload length MSB   */
    frame[pos++] =  pkt->length        & 0xFF;   /* payload length LSB   */

    if (pkt->length > 0)
    {
        memcpy(&frame[pos], pkt->data, pkt->length);
        pos += pkt->length;
    }

    /* CRC covers CMD + LEN_H + LEN_L + DATA (i.e. frame[1] onward) */
    uint16_t crc = crc16_ccitt(&frame[1], (uint16_t)(pkt->length + 3u));

    frame[pos++] = (crc >> 8) & 0xFF;           /* CRC MSB              */
    frame[pos++] =  crc        & 0xFF;           /* CRC LSB              */

    /* ---- Send via active driver ------------------------------------- */
    if (driver_type == SERIAL)
        return drv_serial_tx(&handle_serial_driver, frame, pos);

    if (driver_type == TCP)
        return drv_tcp_tx(&handle_tcp_driver, frame, pos);

    return -1;
}

/**
 * @brief Block until one valid framed packet is received.
 *
 * Reads the byte stream one byte at a time and runs it through a
 * seven-state parser.  The function returns only when a complete,
 * CRC-verified packet has been assembled or the thread is asked
 * to stop.
 *
 * @par Parser states
 *  - State 0 : Scan for @c ':' start delimiter.
 *  - State 1 : Read CMD byte.
 *  - State 2 : Read LEN_H byte.
 *  - State 3 : Read LEN_L byte; validate length; skip to state 5
 *              when length == 0 (no data phase needed).
 *  - State 4 : Read @c length data bytes into @c pkt->data[].
 *  - State 5 : Read CRC_H byte.
 *  - State 6 : Read CRC_L byte; compute and validate CRC; return.
 *
 * @par Error handling
 * On @b any error (length overflow, CRC mismatch) the parser is
 * fully reset via @c parser_reset() before returning the error code.
 * This ensures the next call always starts scanning cleanly for a
 * new @c ':' delimiter regardless of what the previous call did.
 *
 * @param pkt                 Output: populated packet on success.
 * @param thread_running_flag Pointer to the thread control flag.
 *                            The loop exits when this becomes 0.
 *
 * @return Payload byte count (>= 0) on success.
 * @retval -1  Null pointer or thread stopped.
 * @retval -3  Payload length exceeds @c COMM_MAX_DATA.
 * @retval -5  CRC16 mismatch — frame discarded.
 */
int transport_receive(comm_packet_t *pkt, int32_t *thread_running_flag)
{
    if (!pkt || !thread_running_flag)
        return -1;

    uint8_t byte;

    while (*thread_running_flag)
    {
        /* ---- Read one byte from the active driver ------------------- */
        int n = -1;

        if (driver_type == SERIAL)
            n = drv_serial_rx(&handle_serial_driver, &byte, 1);
        else if (driver_type == TCP)
            n = drv_tcp_rx(&handle_tcp_driver, &byte, 1);

        /* No byte available yet — yield and retry */
        if (n <= 0)
            continue;

        /* ---- Feed byte into the parser state machine ---------------- */
        switch (rx_state)
        {
            /* ----------------------------------------------------------
               State 0: hunt for the ':' start-of-frame delimiter.
               Any byte that is not ':' is silently discarded, which
               automatically re-synchronises the parser after noise or
               a framing error on the previous packet.
            ---------------------------------------------------------- */
            case 0:
                if (byte == ':')
                    rx_state = 1;
                break;

            /* ----------------------------------------------------------
               State 1: capture the command byte.
            ---------------------------------------------------------- */
            case 1:
                pkt->command = byte;
                rx_state = 2;
                break;

            /* ----------------------------------------------------------
               State 2: capture the high byte of the payload length.
            ---------------------------------------------------------- */
            case 2:
                rx_length = (uint16_t)((uint16_t)byte << 8);
                rx_state  = 3;
                break;

            /* ----------------------------------------------------------
               State 3: capture the low byte of the payload length.
               Validate that it fits in the packet buffer, then either
               enter the data phase (state 4) or skip straight to the
               CRC phase (state 5) for zero-length packets such as
               RESP_SEG_ACK.
            ---------------------------------------------------------- */
            case 3:
                rx_length |= (uint16_t)byte;

                if (rx_length > (uint16_t)sizeof(pkt->data))
                {
                    /* Payload too large — cannot fit in packet buffer */
                    printf("[ RX ] Frame error: payload length %u exceeds "
                           "buffer %u — discarding\n",
                           rx_length, (uint16_t)sizeof(pkt->data));

                    parser_reset();
                    return -3;
                }

                pkt->length = rx_length;
                rx_index    = 0;

                /* Jump to CRC phase if there are no data bytes */
                rx_state = (rx_length == 0u) ? 5u : 4u;
                break;

            /* ----------------------------------------------------------
               State 4: accumulate payload bytes until rx_length bytes
               have been stored, then advance to the CRC phase.
            ---------------------------------------------------------- */
            case 4:
                pkt->data[rx_index++] = byte;

                if (rx_index == rx_length)
                    rx_state = 5;
                break;

            /* ----------------------------------------------------------
               State 5: capture CRC high byte.
            ---------------------------------------------------------- */
            case 5:
                rx_crc   = (uint16_t)((uint16_t)byte << 8);
                rx_state = 6;
                break;

            /* ----------------------------------------------------------
               State 6: capture CRC low byte, then validate the frame.

               The CRC is computed over the same byte range that
               transport_send() covered: CMD + LEN_H + LEN_L + DATA.
               Building a temporary buffer lets us reuse crc16_ccitt()
               without modifying the received packet struct.
            ---------------------------------------------------------- */
            case 6:
            {
                rx_crc |= (uint16_t)byte;

                /* Save the received CRC into a local variable BEFORE calling
                parser_reset(). parser_reset() zeros rx_crc, so if we compare
                after the reset we are always comparing against 0x0000 — which
                is why every packet was failing CRC validation. Saving to a
                local first means the reset can happen at the clean boundary
                point without destroying the value we still need. */
                uint16_t received_crc = rx_crc;

                /* Build the CRC input buffer: CMD, LEN_H, LEN_L, DATA */
                uint8_t  crc_buf[COMM_MAX_DATA + 3u];
                uint16_t crc_pos = 0;

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

                /* Reset parser state now — safe because received_crc holds
                the value we need and pkt_len holds the length we need.
                From this line onward all module-level parser variables
                are zero regardless of which return path we take. */
                parser_reset();

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
                /* Should never happen — reset to be safe */
                parser_reset();
                break;
        }
    }

    /* Thread stop requested */
    return -1;
}

/**
 * @brief Flush all pending bytes from the serial receive FIFO.
 *
 * Reads and discards bytes until no more are available.  Called once
 * during startup to clear any garbage that arrived before the
 * bootloader was ready to receive.
 *
 * Also resets the RX parser state so @c transport_receive() starts
 * completely clean after the flush.
 *
 * @return 0 always.
 */
int transport_flush(void)
{
    uint8_t tmp;

    /* Drain the hardware FIFO */
    while (drv_serial_rx(&handle_serial_driver, &tmp, 1) > 0)
    {
        /* discard byte */
    }

    /* Reset the software parser so receive starts from a clean state */
    parser_reset();

    return 0;
}