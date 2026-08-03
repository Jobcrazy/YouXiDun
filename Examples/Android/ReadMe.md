# NetGuard Android 原生演示项目

[English](./ReadMe_EN.md)

本项目演示如何在 Android 原生应用中集成并初始化 NetGuard SDK，以及如何通过设备本机 TCP 连接验证 NetGuard 启动的服务。

## 快速开始

1. 使用 Android Studio 打开本目录。
2. 将 `MainActivity.java` 中的 AppId 替换为测试环境提供的有效 AppId。
3. 确保本机已安装 Android SDK 33 和 JDK 11。
4. 在 ARMv7 或 ARM64 Android 设备上构建并运行应用。
5. 确认页面显示 “SDK initialized successfully”。
6. 确认端口后点击 “Connect”，观察每秒发送的 ping 和服务端响应日志。

默认连接地址为 `127.0.0.1:10000`，即 NetGuard 在当前设备上启动的本地服务地址。页面允许修改地址和端口，以便进行调试。

## 模块职责

```text
MainActivity
├─ 初始化 Shield SDK
├─ 管理连接状态和页面交互
└─ 将网络事件显示到日志列表

TcpClient
├─ 建立和关闭 TCP 连接
├─ 每秒发送一个 ping
└─ 持续读取服务端响应

LogAdapter
└─ 分类显示最近 200 条通信日志
```

## 目录结构

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

## SDK 接入

Android SDK 位于 `app/libs/libshield.aar`，通过以下方式初始化：

```java
import com.udptcp.netguard.Shield;

int result = Shield.Init(null, "your-app-id");
```

返回值 `0` 表示初始化成功。

当前工程打包 `armeabi-v7a` 和 `arm64-v8a`。

## TCP 协议

每个数据帧由以下内容组成：

```text
4 字节小端序 payload 长度 + UTF-8 payload
```

完整帧最大为 16 KiB。连接成功后，客户端每秒发送一次：

```json
{"cmd":"ping","id":0,"time":"12:34:56.789"}
```

连接超时为 5 秒。协议错误、发送失败或远端断开时，客户端会关闭 Socket 和后台线程。
