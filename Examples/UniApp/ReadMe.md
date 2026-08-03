# NetGuard UniApp 演示项目

[English](./ReadMe_EN.md)

本项目演示如何在 uni-app x 中集成并初始化 NetGuard Shield SDK，以及如何通过设备本机 TCP 连接验证 NetGuard 启动的服务。

## 快速开始

1. 使用 HBuilderX 4.25 或更高版本打开本目录。
2. 将演示页面中的 AppId 替换为测试环境提供的有效 AppId。
3. 为 Android 或 iOS 制作包含 UTS 插件的自定义基座，或直接进行云打包。
4. 启动应用，确认页面显示“SDK 初始化成功”。
5. 确认端口后点击“连接”，观察每秒发送的 ping 和服务端响应日志。

演示固定连接 `127.0.0.1`。该地址是 NetGuard 在当前设备上启动的本机服务地址，只用于展示，不允许用户修改。

## 插件职责

```text
页面 pages/index/index.uvue
├─ netguard-sdk：初始化 Android/iOS Shield SDK
└─ tcp：连接设备本机 TCP 服务并展示收发日志
```

两个插件相互独立：NetGuard 插件不包含 TCP 实现，TCP 插件也不引用 Shield SDK。

## 目录结构

```text
UniApp/
├─ ReadMe.md
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

## 平台实现

- Android Shield SDK：`libshield.aar`
- iOS Shield SDK：`libshield.a` 和 `Shield.h`
- Android TCP：`java.net.Socket`
- iOS TCP：Apple `Network.framework`
- TCP 协议：4 字节小端长度头 + UTF-8 payload

具体接入方法和接口说明请查看两个插件目录中的中文或英文文档。
