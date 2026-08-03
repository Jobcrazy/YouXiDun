# NetGuard Native iOS Demo

[中文](./ReadMe.md)

This project demonstrates how to integrate and initialize the NetGuard SDK in an Objective-C iOS application and how to verify the local service started by NetGuard through a TCP connection on the same device.

## Quick start

1. Open `iosdemo.xcodeproj` with Xcode.
2. Replace the AppId in `ViewController.m` with a valid AppId for your test environment.
3. Select a valid development team and signing configuration.
4. Build and run the application on an iOS device.
5. Confirm that the log reports “SDK initialized successfully”.
6. Confirm the port, select “Connect”, and inspect the ping and response logs generated every second.

The default endpoint is `127.0.0.1:10000`, where NetGuard starts its local service on the current device. The page allows the host and port to be changed for debugging.

## Component responsibilities

```text
ViewController
├─ Initializes the Shield SDK
├─ Manages connection state and the English UI
└─ Displays the most recent 200 categorized log entries

TCPClient
├─ Manages NSInputStream and NSOutputStream
├─ Sends one ping per second
├─ Handles partial writes and fragmented or coalesced TCP frames
└─ Releases resources consistently after connection or protocol errors
```

## Project structure

```text
iOS/
├─ ReadMe.md
├─ ReadMe_EN.md
├─ iosdemo.xcodeproj/
└─ iosdemo/
   ├─ libshield.a
   ├─ Shield.h
   ├─ TCPClient.h
   ├─ TCPClient.m
   ├─ ViewController.h
   ├─ ViewController.m
   └─ Base.lproj/Main.storyboard
```

## SDK integration

The SDK is integrated through `libshield.a` and `Shield.h`:

```objc
NSInteger result = [[Shield getInstance] Init:nil key:@"your-app-id"];
```

A return value of `0` means initialization succeeded.

## TCP protocol

Each frame consists of:

```text
4-byte little-endian payload length + UTF-8 payload
```

The maximum complete frame size is 16 KiB. After connecting, the client sends one message per second:

```json
{"cmd":"ping","id":0,"time":"12:34:56.789"}
```

The connection timeout is five seconds. The client supports partial writes and fragmented or coalesced frames, and closes its streams and timers after a protocol error, send failure, or remote disconnection.
