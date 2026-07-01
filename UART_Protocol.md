# superConsole IO UART Protocol

This document describes the USART1 control protocol for external host tools.
It is intended for PC-side development and automation.

## Physical / Serial Settings

| Item | Value |
|---|---|
| MCU UART | USART1 |
| MCU TX | PA9 |
| MCU RX | PA10 |
| Baud rate | 38400 bps |
| Data bits | 8 |
| Parity | None |
| Stop bits | 1 |
| Flow control | None |
| Logic level | MCU GPIO logic level |

The firmware uses interrupt-driven 1-byte RX and processes complete lines in the main loop.
USB HID processing remains the priority path.

## Line Format

Commands are ASCII text lines.

| Rule | Detail |
|---|---|
| Encoding | ASCII |
| Line ending | `\r`, `\n`, or `\r\n` |
| Max command line | 63 characters plus terminator |
| Case | Command tokens are case-insensitive |
| Separators | Space or tab |
| Response ending | `\r\n` |

Responses use one of these prefixes:

```text
OK ...
ERR ...
```

On boot, the firmware sends:

```text
OK SCIO UART 1
```

## Implemented Commands

### Ping

```text
PING
```

Response:

```text
OK PONG
```

### Protocol Identity

```text
SCIO?
```

Response:

```text
OK SCIO UART 1
```

### Help

```text
HELP
```

Response:

```text
OK CMDS PING HELP GET BL SET BL <0|20|40|60|80|100> BL <0|20|40|60|80|100> GET BAT
```

### Get Backlight

```text
GET BL
```

or:

```text
BL
```

Response:

```text
OK BL 60
```

### Set Backlight

```text
SET BL 60
```

or shorthand:

```text
BL 60
```

Allowed values:

```text
0 20 40 60 80 100
```

Successful response:

```text
OK BL 60
```

Invalid value response:

```text
ERR RANGE BL 0/20/40/60/80/100
```

Side effect:

- Updates PC6 TIM3_CH1 PWM duty.
- Shows the `LIGHT xx%` large OLED overlay for about 1.6 seconds.
- `0%` sets PWM duty to 0 and is used as the display backlight off state.

### Get Battery Summary

```text
GET BAT
```

Response example:

```text
OK BAT V=7820mV I=120mA P=940mW B=75%
```

If battery percentage is unavailable:

```text
OK BAT V=0mV I=0mA P=0mW B=NA
```

Current values come from INA219 measurements. `B` is a voltage-based firmware estimate, not a fuel-gauge SOC value.

## Error Responses

| Response | Meaning |
|---|---|
| `ERR UNKNOWN` | Unknown command |
| `ERR GET TARGET` | Unknown or missing `GET` target |
| `ERR SET TARGET` | Unknown, missing, or malformed `SET` target |
| `ERR RANGE BL 0/20/40/60/80/100` | Backlight value is outside allowed steps |
| `ERR LINE` | RX line overflow |

## Host-Side Recommendations

- Open the serial port as `38400 8N1`, no flow control.
- Use `\n` or `\r\n` line endings.
- Read and ignore the boot banner `OK SCIO UART 1` before issuing commands.
- Wait for an `OK` or `ERR` line before sending the next command.
- Use a response timeout of at least 100 ms for simple commands.
- For firmware flashing or reset, reopen the serial port and resync with `PING`.

## Python Example

```python
import serial

ser = serial.Serial("/dev/tty.usbserial-XXXX", 38400, timeout=0.2)

def cmd(line: str) -> str:
    ser.write((line + "\n").encode("ascii"))
    return ser.readline().decode("ascii", errors="replace").strip()

print(cmd("PING"))
print(cmd("GET BL"))
print(cmd("SET BL 80"))
print(cmd("GET BAT"))
```

## Planned Extension Namespace

The current parser is intentionally simple and token-based. Recommended future command layout:

| Area | Proposed commands |
|---|---|
| Mouse sensitivity | `GET MS`, `SET MS <20|40|60|80|100>` |
| Battery detail | `GET BAT RAW`, `GET INA219`, `GET MP2672` |
| MP2672A status | `GET CHG`, `GET FAULT`, `GET MP2672 REG03`, `GET MP2672 REG04` |
| Keymap | `GET KEY Cx Ry`, `SET KEY Cx Ry <usage>` |
| Device info | `GET FW`, `GET PINOUT` |

Keep commands short and line-oriented so UART processing remains non-blocking for HID use.
