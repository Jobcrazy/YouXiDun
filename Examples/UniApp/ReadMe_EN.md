# NetGuard UniApp Demo

[中文](./ReadMe.md)

This project demonstrates how to integrate and initialize the NetGuard SDK in uni-app x and how to verify the local service started by NetGuard through a TCP connection on the same device.

## Quick start

1. Open this directory with HBuilderX 4.25 or later.
2. Replace the AppId in the demo page with a valid AppId for your test environment.
3. Build a custom runtime containing the UTS plugins for Android or iOS, or use cloud packaging.
4. Start the application and confirm that the page reports a successful SDK initialization.
5. Confirm the port, select Connect, and inspect the ping and response logs generated every second.

The demo always connects to `127.0.0.1`. This is the address of the local service started by NetGuard on the current device. It is displayed by the page and cannot be edited by the user.

## Plugin responsibilities

```text
Page: pages/index/index.uvue
├─ netguard-sdk: initializes the Android/iOS Shield SDK
└─ tcp: connects to the local TCP service and displays traffic logs
```

The plugins are independent. The NetGuard plugin contains no TCP implementation, and the TCP plugin does not reference the Shield SDK.

## Project structure

```text
UniApp/
├─ ReadMe.md
├─ ReadMe_EN.md
├─ App.uvue
├─ main.uts
├─ manifest.json
├─ pages/
│  └─ index/index.uvue
└─ uni_modules/
   ├─ netguard-sdk/
   │  ├─ ReadMe.md
   │  └─ ReadMe_EN.md
   └─ tcp/
      ├─ ReadMe.md
      └─ ReadMe_EN.md
```

## Platform implementation

- Android Shield SDK: `libshield.aar`
- iOS Shield SDK: `libshield.a` and `Shield.h`
- Android TCP: `java.net.Socket`
- iOS TCP: Apple `Network.framework`
- TCP framing: 4-byte little-endian payload length followed by a UTF-8 payload

See the Chinese or English documentation under each plugin directory for integration and API details.
