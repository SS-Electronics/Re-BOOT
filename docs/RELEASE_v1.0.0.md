# Re-BOOT v1.0.0 — First Release

> **Cross-platform host-side firmware update utility for ARM targets**
> Serial · TCP · CAN  |  Intel HEX  |  GPL-3.0

---

## What is Re-BOOT?

Re-BOOT is a lightweight host-side bootloader client written in C.
It reads an Intel HEX firmware image, connects to a target MCU bootloader over Serial, TCP, or CAN, and flashes the image sector-by-sector using a reliable stop-and-wait protocol with CRC32 verification per sector.

---

## Highlights

- **Three transport interfaces** — UART serial, TCP socket, and CAN (USB-CAN adapter over virtual COM port), all sharing a single packet framing layer
- **Intel HEX parser** — handles type-`00` data records and type-`04` extended linear address records; tracks base and end address across the full image
- **Sector-aligned pipeline** — organises parsed HEX records into flash-sector buffers pre-filled with `0xFF`, consumed segment-by-segment during transfer
- **Stop-and-wait protocol** — every segment (`CMD_PIPELINE_DATA`) waits for `RESP_SEG_ACK` before the next is sent; every sector waits for `RESP_CRC_ACK`/`RESP_CRC_NACK`
- **CRC32 sector verification** — standard CRC32 (poly `0xEDB88320`) computed on the host and confirmed by the target after each flash write
- **Automatic retry** — up to `MAX_RETRY` (2) retransmissions per sector on `RESP_CRC_NACK`; sector offset rewound to byte 0 for a clean retry
- **Address safety check** — aborts before sending any data if the HEX image base address falls below the target's reported `APP_START_ADDRESS`, protecting the bootloader region
- **Event-driven FSM** — 8-state finite state machine with a flat transition table, thread-safe mutex, and internal event queue
- **Dedicated RX thread** — `comm_rx_thread` blocks on `transport_receive()` and pushes packets into a shared queue; the main loop drives the FSM
- **Real-time progress bar** — in-place ASCII bar with percentage, sector count, and current flash address
- **Session log file** — full per-segment and per-sector trace written to `re-boot.log`
- **Cross-platform build** — single `Makefile` supports `OS=Linux` (GCC + pthreads) and `OS=Win` (MinGW-w64 cross-compiler)

---

## Protocol Summary

```
  HOST  →  CMD_RESET_REQ        →  TARGET
  HOST  ←  RESP_TARGET_INFO     ←  TARGET   (flash_addr, sector_size, segment_size)

  for each sector:
    for each segment:
      HOST  →  CMD_PIPELINE_DATA    →  TARGET
      HOST  ←  RESP_SEG_ACK         ←  TARGET
    HOST  →  CMD_PIPELINE_VERIFY  →  TARGET   (CRC32)
    HOST  ←  RESP_CRC_ACK / NACK  ←  TARGET

  HOST  →  CMD_START_APP         →  TARGET
  HOST  ←  RESP_APP_JUMP_ACK     ←  TARGET
```

**Frame format — Serial / TCP** (`':'` + CMD + LEN\_H + LEN\_L + DATA + CRC16-CCITT)
**Frame format — CAN-over-Serial** (`':'` + CMD + LEN + DATA, no CRC — hardware-verified)

---

## What's New in v1.0.0

This is the initial release. Everything listed below was built from scratch.

### Core FSM
- `ST_INIT` → `ST_SEND_RESET` → `ST_BUILD_PIPELINE` → `ST_SEND_WINDOW` → `ST_VERIFY` → `ST_NEXT_SECTOR` → `ST_APP_JUMP` → `ST_DONE`
- `EVT_SEG_ACK` wired directly to `act_send_window` — eliminates the two-event queue burst of an earlier intermediate-hop design
- Thread-safe FSM engine with mutex (`USE_THREAD_SAFE_FSM`)

### Transport Layer
- Serial driver (`drv_serial.c`) — POSIX `termios`, 115200 baud, non-blocking RX
- TCP client driver (`drv_tcp.c`) — connect to IP:port
- CAN-over-Serial framing — compact 3+N byte frames reusing the serial driver for USB-CAN adapters
- CRC16-CCITT (poly `0x1021`) integrity on Serial/TCP paths

### Pipeline Builder
- Sector-aligned buffer allocation from parsed HEX records
- `pipeline_next_segment()` — skips unprogrammed bytes, advances offset
- `pipeline_sector_crc()` — standard CRC32 over the full sector buffer

### HEX File Parser
- Type-`00` and type-`04` record support
- Tracks `hex_base_address` and `hex_end_address` for address compatibility checks

### Build System
- Recursive sub-Makefile per module (`init`, `comm`, `driver`, `utility`, `thread`)
- `OS=Linux` — `gcc`, `-pthread`, output `re-boot`
- `OS=Win` — `x86_64-w64-mingw32-gcc`, `-lws2_32`, output `re-boot.exe`
- `make docs` — Doxygen HTML generation

---

## Getting Started

```bash
# Clone
git clone https://github.com/subhajitroy005/Re-BOOT.git
cd Re-BOOT

# Build (Linux)
make

# Flash a target over UART
./re-boot -f firmware.hex -n 1 -c serial -i /dev/ttyACM0

# Flash over TCP
./re-boot -f firmware.hex -n 1 -c tcp -i 192.168.1.100 -p 5000

# Flash over CAN (USB-CAN adapter)
./re-boot -f firmware.hex -n 1 -c can -i /dev/ttyUSB0
```

Build for Windows on Linux:

```bash
sudo apt install mingw-w64
make OS=Win
```

Full build, usage, and protocol documentation in [README.md](../README.md).

---

## Known Limitations

- **Baud rate fixed at 115200** — Serial and CAN paths always open at 115200 baud; there is no CLI flag to override it.
- **Single-ACK stop-and-wait only** — the `in_flight` field in `bootloader_ctx_t` is reserved for a future sliding-window extension but is unused in this release.
- **Sector order not sorted** — sectors are built in the order they are first encountered in the HEX file. Out-of-order HEX records across sector boundaries may produce an unexpected sector sequence.
- **No timeout / retransmit on missing ACK** — if the target stops responding the FSM blocks indefinitely. A watchdog timer is planned for a future release.
- **CAN payload capped at 8 bytes** — `CMD_PIPELINE_DATA` payloads larger than `CAN_MAX_PAYLOAD` are truncated; `segment_size` reported by the target must be ≤ 8 when using CAN.

---

## Repository

| | |
|---|---|
| **Author** | Subhajit Roy — subhajitroy005@gmail.com |
| **Organisation** | SS-Electronics |
| **License** | GNU General Public License v3.0 |
| **Language** | C (C11) |
| **Target platform** | ARM MCU (target-side bootloader required) |
| **Host platform** | Linux, Windows (MinGW-w64) |
