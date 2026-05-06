# Re-BOOT

[![Build](https://github.com/SS-Electronics/Re-BOOT/actions/workflows/build.yml/badge.svg)](https://github.com/SS-Electronics/Re-BOOT/actions/workflows/build.yml)
[![Issues](https://img.shields.io/github/issues/SS-Electronics/Re-BOOT)](https://github.com/SS-Electronics/Re-BOOT/issues)
[![Language](https://img.shields.io/badge/Language-C-blue.svg)](https://en.wikipedia.org/wiki/C_(programming_language))
[![Type](https://img.shields.io/badge/Type-Bootloader-orange.svg)](https://github.com/SS-Electronics/Re-BOOT)
[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)

A cross-platform host-side firmware update utility that transfers Intel HEX images to an embedded target over **Serial (UART)**, **TCP**, **UDP**, or **CAN** using a lightweight, event-driven bootloader protocol.

```
  ╔════════════════════════════════════════╗
  ║           Re-BOOT Application          ║
  ║                                        ║
  ║   Platform   : ARM                     ║
  ║   Support    : Intel HEX               ║
  ║   Version    : v1.0.0                  ║
  ║   Developer  : SS-Electronics          ║
  ║   License    : GPL-3.0                 ║
  ╚════════════════════════════════════════╝
```

---

## Table of Contents

1. [Features](#features)
2. [Directory Structure](#directory-structure)
3. [Architecture Overview](#architecture-overview)
4. [Protocol Reference](#protocol-reference)
5. [Frame Formats](#frame-formats)
6. [FSM State Machine](#fsm-state-machine)
7. [Build](#build)
8. [Usage](#usage)
9. [Configuration](#configuration)
10. [Log File](#log-file)
11. [License](#license)

---

## Features

- Flash ARM targets via **UART serial**, **TCP socket**, **UDP socket**, or **CAN** (USB-CAN adapter as virtual COM port)
- **Node ID routing** — the node ID (`-n`) is encoded directly in the command byte (`CMD_RESET_REQ + node_id`) so multiple targets can share the same bus; only the matching node responds
- Parses **Intel HEX** files including extended linear address records (type `0x04`)
- Sector-aligned firmware pipeline — organises the HEX image into flash-sector buffers prefilled with `0xFF`
- Segment-by-segment stop-and-wait transfer with per-segment ACK
- Per-sector **CRC32** verification — the target flashes and confirms before the host advances
- Automatic **retry** on CRC NACK (configurable, default `MAX_RETRY = 2`)
- **Address safety check** — aborts if the HEX image would overlap the bootloader region
- Real-time ASCII **progress bar** on the terminal
- Session **log file** (`re-boot.log`) with full transfer trace
- Clean **SIGINT** (`Ctrl+C`) handling
- Thread-safe FSM with mutex protection

---

## Directory Structure

```
Re-BOOT/
├── init/
│   ├── main.c              # Entry point — init, main loop, resource management
│   ├── fsm_table.c         # State objects and transition table
│   └── fsm_actions.c       # Action function implementations
│
├── comm/
│   ├── transport_layer.c   # Driver-independent framing (Serial / TCP / UDP / CAN)
│   └── comm_manager.c      # Dedicated RX thread → packet queue
│
├── driver/
│   ├── drv_serial.c        # POSIX / Win32 serial (termios / COM) driver
│   ├── drv_tcp.c           # TCP client driver
│   ├── drv_udp.c           # UDP socket driver
│   └── drv_file_write.c    # Log file writer
│
├── utility/
│   ├── pipeline.c          # HEX → sector pipeline builder + segment iterator
│   ├── fsm.c               # Generic FSM engine (dispatch, run, event queue)
│   ├── file_mgmt.c         # HEX file parser and CLI argument parser
│   ├── mem_mgmt.c          # Memory pool allocator
│   └── data_conversion.c   # Hex string to integer conversion
│
├── thread/
│   ├── threads.c           # pthread wrapper
│   └── queues.c            # Thread-safe pointer queue
│
├── include/                # All public header files
│   ├── drv_serial.h
│   ├── drv_tcp.h
│   ├── drv_udp.h
│   └── ...
│
├── config/
│   ├── app_config.h        # Runtime tuning constants (SERIAL/TCP/UDP/CAN defines)
│   └── bl_protocol_config.h# Protocol command and response codes
│
└── docs/
    ├── statechart.puml     # PlantUML FSM source
    ├── Re-BOOT Firmware Update FSM.svg
    └── Re-BOOT Firmware Update FSM.png
```

---

## Architecture Overview

```
  ┌─────────────────────────────────────────────────────────┐
  │                        main.c                           │
  │                                                         │
  │  1. Parse CLI args          5. FSM main loop            │
  │  2. Open transport             ┌─────────────────────┐  │
  │  3. Parse HEX file             │ act_fsm_signal_gen  │  │
  │  4. Build mem pool             │   queue_try_pop()   │  │
  │                                │   fsm_dispatch()    │  │
  │                                │   fsm_run()         │  │
  │                                └─────────────────────┘  │
  └─────────────────────────────────────────────────────────┘
              │                          ▲
              │ thread_create            │ queue_push
              ▼                         │
  ┌───────────────────────┐    ┌────────┴──────────┐
  │   comm_rx_thread      │───►│  RX packet queue  │
  │  transport_receive()  │    │  (handle_queue_   │
  │  (blocking, 1 byte    │    │  receive_packets) │
  │   at a time)          │    └───────────────────┘
  └───────────────────────┘
              │
  ┌───────────┴────────────────────────────────────────────┐
  │                   transport_layer.c                    │
  │  Serial/TCP/UDP: ':' CMD LEN_H LEN_L DATA CRC_H CRC_L │
  │  CAN:            ':' CMD LEN DATA                      │
  └──────┬─────────────────┬──────────────┬───────────────┘
         │                 │              │
  ┌──────┴──────┐   ┌──────┴──────┐  ┌───┴──────┐
  │ drv_serial  │   │  drv_tcp    │  │ drv_udp  │
  │ (SERIAL/CAN)│   │  (TCP)      │  │ (UDP)    │
  └─────────────┘   └─────────────┘  └──────────┘
```

**Threading model:** The main thread drives the FSM event loop. A single background thread (`comm_rx_thread`) blocks on `transport_receive()`, heap-allocates each valid packet, and pushes it into a shared lock-free queue. The main loop pops packets, maps them to FSM events, and dispatches.

---

## Protocol Reference

### Commands — Host → Target

| Code | Name | Payload |
|------|------|---------|
| `0x10 + node_id` | `CMD_RESET_REQ + node_id` | 1 byte: `0x01` (flag) |
| `0x11` | `CMD_PIPELINE_DATA` | 4 bytes addr (big-endian) + N bytes data (N = segment_size) |
| `0x12` | `CMD_ADDR_UPDATE` | Reserved |
| `0x13` | `CMD_PIPELINE_VERIFY` | 4 bytes: CRC32 of sector (big-endian) |
| `0x14` | `CMD_START_APP` | 1 byte: `0x01` (flag) |

> **Node ID encoding:** The node ID is added directly to the `CMD_RESET_REQ` base code (`0x10`). For example, node 3 sends command byte `0x13`. This allows multiple nodes to share the same bus without payload inspection.

### Responses — Target → Host

| Code | Name | Payload |
|------|------|---------|
| `0x30 + node_id` | `RESP_TARGET_INFO + node_id` | 8 bytes: `flash_addr`(4B) + `sector_size`(2B) + `segment_size`(2B), all big-endian |
| `0x31` | `RESP_SEG_ACK` | None |
| `0x32` | `RESP_SEG_NACK` | None |
| `0x33` | `RESP_CRC_ACK` | None — sector written and verified OK |
| `0x34` | `RESP_CRC_NACK` | None — sector write failed, host will retry |
| `0x35` | `RESP_SECTOR_WR_ACK` | None |
| `0x36` | `RESP_SECTOR_WR_NACK` | None |
| `0x37` | `RESP_APP_JUMP_ACK` | None — target jumped to application |
| `0x38` | `RESP_APP_JUMP_NACK` | None |
| `0x39` | `RESP_PIPE_INFO` | Reserved |
| `0x40` | `RESP_PIPELINE_CRC` | Reserved |

> **Node ID filtering:** `act_fsm_signal_generation()` matches the received command byte against `RESP_TARGET_INFO + node_id` at runtime. Packets intended for a different node are silently discarded.

### Transfer Sequence

```
 HOST                                              TARGET
  │                                                  │
  │── CMD_RESET_REQ + node_id ──────────────────►    │
  │◄─ RESP_TARGET_INFO + node_id ──────────────────  │  flash_addr, sector_size, segment_size
  │                                                  │
  │  [host builds pipeline from HEX records]         │
  │                                                  │
  │── CMD_PIPELINE_DATA (0x11) ───────────────────►  │  segment 0 of sector 0
  │◄─ RESP_SEG_ACK (0x31) ────────────────────────   │
  │── CMD_PIPELINE_DATA (0x11) ───────────────────►  │  segment 1 of sector 0
  │◄─ RESP_SEG_ACK (0x31) ────────────────────────   │
  │    ... (repeat for all segments) ...              │
  │── CMD_PIPELINE_VERIFY (0x13) ─────────────────►  │  CRC32 of sector
  │◄─ RESP_CRC_ACK (0x33) ────────────────────────   │  sector written OK
  │                                                  │
  │    ... (repeat for every sector) ...              │
  │                                                  │
  │── CMD_START_APP (0x14) ───────────────────────►  │
  │◄─ RESP_APP_JUMP_ACK (0x37) ───────────────────   │
```

On `RESP_CRC_NACK` the host resets the sector offset and retransmits the complete sector from byte 0. Maximum `MAX_RETRY` (2) attempts per sector before aborting.

---

## Frame Formats

### Serial / TCP / UDP

Byte-stream framing with CRC16-CCITT integrity check (polynomial `0x1021`, init `0xFFFF`).

```
 ┌─────┬─────┬───────┬───────┬──────────────┬───────┬───────┐
 │ ':' │ CMD │ LEN_H │ LEN_L │ DATA[0..N-1] │ CRC_H │ CRC_L │
 └─────┴─────┴───────┴───────┴──────────────┴───────┴───────┘
   1 B   1 B    1 B     1 B       N bytes      1 B     1 B
```

CRC is computed over `CMD + LEN_H + LEN_L + DATA`.

For UDP each call to `drv_udp_tx()` sends the complete framed packet as a single datagram. The receiver processes the bytes within the datagram through the same byte-wise state machine as Serial and TCP.

### CAN-over-Serial

Compact framing over a USB-CAN adapter's virtual serial port. No CRC appended — CAN hardware provides a 15-bit frame CRC.

```
 ┌─────┬─────┬─────┬──────────────────┐
 │ ':' │ CMD │ LEN │ DATA[0..LEN-1]   │
 └─────┴─────┴─────┴──────────────────┘
   1 B   1 B   1 B      0–8 bytes
```

Maximum payload: `CAN_MAX_PAYLOAD = 8` bytes.

The `':'` start delimiter is common to all four variants, enabling easy re-synchronisation on a noisy line.

---

## FSM State Machine

The firmware update sequence is driven by an event-driven FSM defined in `init/fsm_table.c` and implemented in `init/fsm_actions.c`.

```
ST_INIT
  │  EVT_START / act_send_reset  [TX: CMD_RESET_REQ + node_id]
  ▼
ST_SEND_RESET
  │  EVT_TARGET_INFO / act_target_info  [RX: RESP_TARGET_INFO + node_id]
  ▼
ST_BUILD_PIPELINE
  │  EVT_START / act_build_pipeline  [pipeline_build()]
  ▼
ST_SEND_WINDOW ◄────────────────────────────────────────────────┐
  │  EVT_START   / act_send_window  [TX: CMD_PIPELINE_DATA]      │
  │  EVT_SEG_ACK / act_send_window  [RX: RESP_SEG_ACK, next seg] │ (retry)
  │  EVT_SECTOR_END / act_crc_verify  [TX: CMD_PIPELINE_VERIFY]  │
  ▼                                                              │
ST_VERIFY                                                        │
  │  EVT_CRC_OK   / act_next_sector  [RX: RESP_CRC_ACK]          │
  │  EVT_CRC_NACK / act_crc_nack     [RX: RESP_CRC_NACK] ───────►┘
  ▼
ST_NEXT_SECTOR
  │  EVT_START            / act_send_window  [more sectors]
  │  EVT_ALL_SECTORS_DONE / act_app_jump  [TX: CMD_START_APP]
  ▼
ST_APP_JUMP
  │  EVT_APP_ACK / act_done  [RX: RESP_APP_JUMP_ACK]
  ▼
ST_DONE
```

**Key design points:**
- **Node ID in command byte:** `act_send_reset()` sends `CMD_RESET_REQ + node_id`. `act_fsm_signal_generation()` checks `pkt->command == RESP_TARGET_INFO + node_id` with a runtime comparison (not a `case` label) before dispatching `EVT_TARGET_INFO`. Packets from other nodes are silently discarded.
- `EVT_SEG_ACK` is wired *directly* to `act_send_window` — no intermediate hop. This makes each ACK→send transition atomic and eliminates the double-send race that a two-event design would create.
- Address safety check in `act_target_info`: aborts if `hex_base_address < APP_START` (would overwrite the bootloader) or if the HEX image is empty.
- Retry rewind: on NACK, `ctx->offset` and `segments_sent` are rewound by one sector so the progress bar never exceeds 100%.

The full statechart is available as SVG and PNG in `docs/`.

---

## Build

The project ships a recursive `make`-based build system. The root [Makefile](Makefile) includes one sub-Makefile per module directory and supports two OS targets selected with the `OS` variable.

| `OS=` value | Compiler | Extra flags | Output |
|-------------|----------|-------------|--------|
| `Linux` *(default)* | `gcc` | `-D__linux__ -pthread` | `re-boot` |
| `Win` | `x86_64-w64-mingw32-gcc` | `-D_WIN64 -lws2_32` | `re-boot.exe` |

All intermediate objects are placed under `build/`.

---

### Linux

#### Prerequisites

| Tool | Install (Debian / Ubuntu) |
|------|--------------------------|
| GCC | `sudo apt install build-essential` |
| GNU Make | included in `build-essential` |

#### Build

```bash
# default — OS=Linux is implied
make

# explicit
make OS=Linux
```

#### Clean

```bash
make clean
```

#### Expected output

```
##############################################
Bulding sources...
##############################################
Building C Source driver/drv_file_write.c ...
Building C Source driver/drv_serial.c ...
Building C Source driver/drv_tcp.c ...
Building C Source driver/drv_udp.c ...
...
**********************************************
Linking executable for Linux...
**********************************************
##############################################

Build completed!:   re-boot

   text    data     bss     dec     hex filename
  42768     648     432   43848    ab48 re-boot

##############################################
```

---

### Windows

The Windows binary is built with the **MinGW-w64** cross-compiler. Two workflows are supported:

#### Option A — Cross-compile on Linux (recommended)

Install the cross-toolchain, then pass `OS=Win` to make:

```bash
# Debian / Ubuntu
sudo apt install mingw-w64

# Build
make OS=Win
```

This produces `re-boot.exe` which runs on 64-bit Windows without any additional runtime.

#### Option B — Native build on Windows with MSYS2

1. Download and install [MSYS2](https://www.msys2.org/).
2. Open the **MSYS2 MinGW 64-bit** shell and install the toolchain:

```bash
pacman -S mingw-w64-x86_64-gcc make
```

3. Clone / copy the repository into the MSYS2 environment, then build:

```bash
make OS=Win
```

#### Clean (both options)

```bash
make clean
```

---

### Doxygen API Documentation

```bash
# Generate HTML docs into docs/generated/html/
make docs

# Remove generated docs
make clean-docs
```

Requires [Doxygen](https://www.doxygen.nl/) to be installed and on `PATH`.

---

## Usage

```
re-boot -f <hex_file> -n <node_id> -c <interface> -i <port_or_ip> [options]
```

### Arguments

| Flag | Value | Required | Description |
|------|-------|----------|-------------|
| `-f` | `<path>` | Yes | Path to the Intel HEX firmware file |
| `-n` | `<id>` | Yes | Target node ID — encoded into `CMD_RESET_REQ` and checked against `RESP_TARGET_INFO` |
| `-c` | `serial` \| `tcp` \| `udp` \| `can` | Yes | Communication interface type |
| `-i` | `<device_or_ip>` | Yes | Serial device (`ttyACM0`, `COM15`) or IP address (TCP/UDP) |
| `-p` | `<port>` | TCP / UDP only | Remote port number |
| `-t` | `<count>` | No | Maximum retries per sector (overrides `MAX_RETRY`) |
| `-r` | `0` \| `1` | No | Reset flag |
| `-v` | `1` \| `2` \| `3` | No | Verbose level (3 = print every HEX record) |

### Examples

**Serial (UART):**
```bash
./re-boot -f firmware.hex -n 1 -c serial -i /dev/ttyACM0
```

**TCP:**
```bash
./re-boot -f firmware.hex -n 1 -c tcp -i 192.168.1.100 -p 5000
```

**UDP:**
```bash
./re-boot -f firmware.hex -n 1 -c udp -i 192.168.1.100 -p 5000
```

**CAN (USB-CAN adapter on virtual COM port):**
```bash
./re-boot -f firmware.hex -n 1 -c can -i /dev/ttyUSB0
```

### Terminal output

```
  [ Re-BOOT ] Connecting to target (node 1) ...
  [ Re-BOOT ] Target connected  (node 1)
              Flash start : 0x08004000
              Sector size : 2048 bytes
              Segment size: 64 bytes
              HEX range   : 0x08004000 -> 0x0800C200
              Addr check  : PASS  (0x08004000 >= 0x08004000)

  [ Re-BOOT ] Image ready — 5 sectors (~160 segments)

  Flashing  [=========>          ]  47%  sector 2/5  0x08006000
  [ OK ]  Sector  2  0x08006000  written and verified
  ...

  ╔══════════════════════════════════════╗
  ║   Re-BOOT : Firmware update DONE !   ║
  ╚══════════════════════════════════════╝
```

---

## Configuration

All compile-time constants live in `config/app_config.h` and `config/bl_protocol_config.h`.

| Constant | Default | Description |
|---|---|---|
| `SERIAL` | `1` | Interface type selector for serial |
| `TCP` | `2` | Interface type selector for TCP |
| `CAN` | `3` | Interface type selector for CAN |
| `UDP` | `4` | Interface type selector for UDP |
| `COMM_MAX_DATA` | `64` | Maximum data payload bytes per packet |
| `CAN_MAX_PAYLOAD` | `8` | Maximum CAN frame data bytes |
| `QSIZE` | `128` | FSM internal event queue depth |
| `MAX_RETRY` | `2` | Maximum sector retransmissions before abort |
| `USE_THREAD_SAFE_FSM` | `1` | Enable mutex in FSM (`0` to disable) |
| `MAX_CHAR_PER_LINE` | `260` | Maximum characters per HEX file line |

---

## Log File

Re-BOOT writes a session trace to `re-boot.log` in the working directory. The log captures every protocol event, including:

- Number of HEX records parsed and address range
- Target node ID and flash parameters received
- Address compatibility check result
- Per-segment transmissions: sector index, flash address, byte count, first 4 data bytes
- Per-sector CRC32 values sent
- Sector write confirmations and retry events
- Final completion summary (sector count, total segments sent)

Example log entries:
```
[RESET] CMD_RESET_REQ+1 (0x11) sent
[TARGET] node_id=1  flash=0x08004000  sector=2048 B  segment=64 B
[TARGET] Addr check PASS: HEX=0x08004000 >= target=0x08004000
[PIPELINE] Built: 5 sectors, ~160 total segments
[SEG TX ] sector=0  addr=0x08004000  len= 64 B  data=[DE AD BE EF ...]  total_sent=1
[VERIFY ] sector=0  addr=0x08004000  CRC32=0xA1B2C3D4
[SECTOR ] sector=0 addr=0x08004000  WRITTEN OK  (1/5)
[DONE   ] Firmware upload complete. Sectors=5  Segments=160
```

---

## License

Re-BOOT is free software distributed under the **GNU General Public License v3.0 or later**.

See [LICENSE](LICENSE) for the full text, or visit <https://www.gnu.org/licenses/>.
