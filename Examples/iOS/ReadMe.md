# NetGuard iOS 原生演示项目

[English](./ReadMe_EN.md)

本项目演示如何在 Objective-C iOS 应用中集成并初始化 NetGuard SDK，以及如何通过设备本机 TCP 连接验证 NetGuard 启动的服务。

## 快速开始

1. 使用 Xcode 打开 `iosdemo.xcodeproj`。
2. 将 `ViewController.m` 中的 AppId 替换为测试环境提供的有效 AppId。
3. 选择有效的开发团队和签名配置。
4. 在 iOS 设备上构建并运行应用。
5. 确认日志显示 “SDK initialized successfully”。
6. 确认端口后点击 “Connect”，观察每秒发送的 ping 和服务端响应日志。

默认连接地址为 `127.0.0.1:10000`，即 NetGuard 在当前设备上启动的本地服务地址。页面允许修改地址和端口，以便进行调试。

## 模块职责

```text
ViewController
├─ 初始化 Shield SDK
├─ 管理连接状态和英文界面
└─ 分类显示最近 200 条通信日志

TCPClient
├─ 管理 NSInputStream 和 NSOutputStream
├─ 每秒发送一个 ping
├─ 处理部分写入、TCP 拆包和粘包
└─ 在连接或协议错误后统一释放资源
```

## 目录结构

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

## SDK 接入

SDK 通过 `libshield.a` 和 `Shield.h` 接入：

```objc
NSInteger result = [[Shield getInstance] Init:nil key:@"your-app-id"];
```

返回值 `0` 表示初始化成功。

## TCP 协议

每个数据帧由以下内容组成：

```text
4 字节小端序 payload 长度 + UTF-8 payload
```

完整帧最大为 16 KiB。连接成功后，客户端每秒发送一次：

```json
{"cmd":"ping","id":0,"time":"12:34:56.789"}
```

连接超时为 5 秒。客户端支持部分写入、多个连续数据帧和不完整数据帧，并会在协议错误、发送失败或远端断开后关闭 Stream 和定时器。
