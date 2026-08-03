# NetGuard Native Windows Demo

[中文](./ReadMe.md)

This pure Win32 C project demonstrates how to initialize the NetGuard Shield SDK and verify the local service started by NetGuard through a TCP connection on the same machine.

## Build

This project is compiled with **GCC for Windows**, available from WinLibs:

https://winlibs.com/

The Makefile assumes that the 32-bit GCC for Windows distribution has been extracted to:

```text
D:\MinGW32\
```

It therefore uses these tools by default:

```text
D:\MinGW32\bin\gcc.exe
D:\MinGW32\bin\windres.exe
D:\MinGW32\bin\mingw32-make.exe
```

Build the project with:

```powershell
cd Examples\Windows
D:\MinGW32\bin\mingw32-make.exe
```

If GCC for Windows is installed elsewhere, change these two variables at the top of the `Makefile`:

```makefile
CC := D:/MinGW32/bin/gcc.exe
WINDRES := D:/MinGW32/bin/windres.exe
```

Replace them with the actual paths to `gcc.exe` and `windres.exe`, then run the `mingw32-make.exe` from that same toolchain. Use forward slashes `/` for paths inside the Makefile.

The output is:

```text
Release/WinDemo.exe
```

Keep `WinDemo.exe` and `Shield.dll` in the same `Release` directory when running the demo.

## Features

- Loads `Shield.dll` with `LoadLibrary` and calls its exported `Init` function.
- Uses `res/Shield.ico` as the window and executable icon.
- Provides editable Host and Port fields, defaulting to `127.69.88.11:10000`.
- Displays SDK initialization, connection state, and communication logs.
- Sends one ping per second and continuously receives server responses.
- Displays the most recent 200 categorized log entries.
- Supports a five-second connection timeout, partial sends, and fragmented or coalesced TCP frames.
- Releases the socket and worker thread consistently after connection, protocol, or send errors.

## Code structure

```text
Windows/
├─ Makefile
├─ ReadMe.md
├─ ReadMe_EN.md
├─ include/
│  ├─ shield_sdk.h
│  └─ tcp_client.h
├─ src/
│  ├─ main.c          # Win32 UI, logs, and application lifecycle
│  ├─ shield_sdk.c    # Dynamic Shield.dll loading
│  └─ tcp_client.c    # Winsock TCP client
├─ res/
│  ├─ Shield.ico
│  └─ resource.rc
└─ Release/
   ├─ Shield.dll
   └─ WinDemo.exe
```

## TCP protocol

Each frame consists of a 4-byte little-endian payload length followed by a UTF-8 payload. The maximum complete frame size is 16 KiB. After connecting, the client sends:

```json
{"cmd":"ping","id":0,"time":"12:34:56.789"}
```
