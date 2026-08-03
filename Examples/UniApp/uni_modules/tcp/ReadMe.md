# TCP Client 使用说明

[English](./ReadMe_EN.md)

这是一个独立的 Android/iOS TCP 客户端插件。

## 1. 使用方法

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

事件类型包括 `connected`、`disconnected`、`sent`、`received` 和 `error`。

本演示固定连接 `127.0.0.1`，该地址对应 NetGuard 在当前设备上启动的服务。页面只展示地址，不允许用户修改；本机回环连接不需要 iOS 局域网访问权限。

## 2. 通信协议

每个数据帧由 4 字节小端序 payload 长度和 UTF-8 payload 组成。连接成功后，演示程序每秒发送一条 ping JSON，并持续读取服务端返回的数据帧。

## 3. 平台实现

- Android 使用 `java.net.Socket`，发送和接收运行在后台工作线程。
- iOS 使用 Apple `Network.framework` 和串行调度队列。
- 持续事件回调使用 `@UTSJS.keepAlive` 保活。
- 页面事件在主线程回调，避免后台线程直接修改界面状态。
- iOS Swift 原生混编要求 HBuilderX 4.25 或更高版本。

## 4. 目录结构

```text
tcp/
├─ package.json
├─ ReadMe.md
├─ ReadMe_EN.md
└─ utssdk/
   ├─ interface.uts
   ├─ app-android/
   │  ├─ AndroidManifest.xml
   │  ├─ config.json
   │  └─ index.uts
   └─ app-ios/
      ├─ config.json
      ├─ index.uts
      └─ TcpNative.swift
```

TCP 插件不引用 Shield SDK，与 `netguard-sdk` 保持独立。
