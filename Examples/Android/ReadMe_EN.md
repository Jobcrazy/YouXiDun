# NetGuard Native Android Demo

[中文](./ReadMe.md)

This project demonstrates how to integrate and initialize the NetGuard Shield SDK in a native Android application and how to verify the local service started by NetGuard through a TCP connection on the same device.

## Quick start

1. Open this directory with Android Studio.
2. Replace the AppId in `MainActivity.java` with a valid AppId for your test environment.
3. Make sure Android SDK 33 and JDK 11 are installed.
4. Build and run the application on an ARMv7 or ARM64 Android device.
5. Confirm that the page reports “SDK initialized successfully”.
6. Confirm the port, select “Connect”, and inspect the ping and response logs generated every second.

The default endpoint is `127.0.0.1:10000`, where NetGuard starts its local service on the current device. The page allows the host and port to be changed for debugging.

## Component responsibilities

```text
MainActivity
├─ Initializes the Shield SDK
├─ Manages connection state and UI actions
└─ Displays network events in the log list

TcpClient
├─ Opens and closes the TCP connection
├─ Sends one ping per second
└─ Continuously reads server responses

LogAdapter
└─ Displays the most recent 200 categorized log entries
```

## Project structure

```text
Android/
├─ ReadMe.md
├─ ReadMe_EN.md
├─ build.gradle
├─ settings.gradle
└─ app/
   ├─ build.gradle
   ├─ libs/libshield.aar
   └─ src/main/
      ├─ AndroidManifest.xml
      ├─ java/com/example/ping/
      │  ├─ MainActivity.java
      │  ├─ TcpClient.java
      │  └─ LogAdapter.java
      └─ res/
```

## SDK integration

The Android SDK is stored at `app/libs/libshield.aar` and initialized as follows:

```java
import com.udptcp.netguard.Shield;

int result = Shield.Init(null, "your-app-id");
```

A return value of `0` means initialization succeeded. Do not hard-code a production AppId in a released client; fetch it from your application backend after authentication.

The current project packages `armeabi-v7a` and `arm64-v8a` and supports Android API 19 or later.

## TCP protocol

Each frame consists of:

```text
4-byte little-endian payload length + UTF-8 payload
```

The maximum complete frame size is 16 KiB. After connecting, the client sends one message per second:

```json
{"cmd":"ping","id":0,"time":"12:34:56.789"}
```

The connection timeout is five seconds. The client closes its socket and background workers after a protocol error, send failure, or remote disconnection.

