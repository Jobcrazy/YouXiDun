# TCP Client

[中文](./ReadMe.md)

An independent TCP client plugin for Android and iOS.

## Usage

```uts
import { connectTcp, disconnectTcp, TcpEvent } from '@/uni_modules/tcp'

connectTcp({
	host: '127.0.0.1',
	port: 10000,
	event: (event : TcpEvent) => {
		console.log(event.type, event.message)
	}
})

disconnectTcp()
```

Event types are `connected`, `disconnected`, `sent`, `received`, and `error`.

## Protocol

Each frame consists of a 4-byte little-endian payload length followed by a UTF-8 payload. After connecting, the demo sends one ping JSON message per second and continuously reads response frames.

## Platform implementation

- Android uses `java.net.Socket` with dedicated send and receive workers.
- iOS uses Apple's `Network.framework` and a serial dispatch queue.
- Repeated callbacks are retained with `@UTSJS.keepAlive` and delivered to the UI on the main thread.
- The iOS Swift source integration requires HBuilderX 4.25 or later.

This demo always connects to `127.0.0.1`, where NetGuard starts its service on the same device. The address is displayed by the page but cannot be edited by the user. No iOS local-network permission is required for this loopback connection.
