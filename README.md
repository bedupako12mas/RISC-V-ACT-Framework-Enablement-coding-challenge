# UART Interface Test — Linux termios

A C program that initializes and configures a UART interface on Linux using the `termios` API. It transmits a test message over the specified serial port and prints any incoming data to the console using a `select()`-based timeout.

---

## Requirements

- Linux
- GCC
- `make`
- A USB-UART adapter and serial device (e.g. `/dev/ttyUSB0`)

---

## Build

```bash
make
```

To clean the build artifact:

```bash
make clean
```

---

## Usage

```
./prog <device>
```

| Argument | Description |
|---|---|
| `<device>` | Path to the serial device (e.g. `/dev/ttyUSB0`, `/dev/ttyS0`) |

---

## Running on Real Hardware

Connect a USB-UART adapter to your machine and identify the device:

```bash
ls /dev/ttyUSB*
# or
dmesg | tail -20
```

If you get a permission error, add your user to the `dialout` group:

```bash
sudo usermod -aG dialout $USER
# log out and back in for this to take effect
```

Run the program:

```bash
./prog /dev/ttyUSB0
```

The program transmits `Hello from UART` at **115200 baud, 8N1** and waits up to 3 seconds for a response. The device on the other end must be configured with the same parameters, or received data will be garbled.

**Loopback test** — to verify TX and RX without a second device, short the TX and RX pins on your UART adapter with a jumper wire. Your transmitted message will loop back and be printed as received data.

---

## Expected Output

```
Opened /dev/ttyUSB0 at 115200 baud (8N1)
Transmitted 17 bytes: Hello from UART
Waiting for incoming data (3s timeout)...
Received 6 bytes: hello                  ← response from the connected device
No data received within timeout.          ← printed if nothing arrives
```

---

## Error Handling

| Scenario | Behaviour |
|---|---|
| Device path does not exist | Prints `cannot open '/dev/...': No such file or directory` and exits |
| Insufficient permissions | Prints `cannot open '/dev/...': Permission denied` and exits |
| `write()` failure | Prints error and exits, restoring terminal settings |
| `read()` failure | Prints error and breaks out of the receive loop |
| Ctrl+C / SIGTERM | Restores original terminal settings before exiting |
