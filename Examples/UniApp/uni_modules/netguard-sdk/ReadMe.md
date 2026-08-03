# NetGuard SDK：UniApp 使用示例

[English](./ReadMe_EN.md)

本插件用于演示如何在 uni-app / uni-app x 项目中初始化 NetGuard Shield SDK，同时支持 Android 和 iOS。

## 1. 引入并初始化

从插件根路径导入 `Init`：

```uts
import { Init } from '@/uni_modules/netguard-sdk'
```

在页面加载或应用启动时传入从业务后台获取的 AppId：

```uts
const result = Init('your-app-id')

if (result == 0) {
	console.log('NetGuard SDK 初始化成功')
} else {
	console.error('NetGuard SDK 初始化失败，错误码：' + result)
}
```

示例项目在 `pages/index/index.uvue` 的 `onLoad` 中初始化 SDK。正式 AppId 不应硬编码在发布的客户端中，建议登录后从业务后台获取。

## 2. 平台要求

- Android 最低版本由 `utssdk/app-android/config.json` 声明。
- iOS 最低版本为 iOS 12.0。
- Android 使用 `utssdk/app-android/libs/libshield.aar`。
- iOS 使用 `utssdk/app-ios/Libs/libshield.a` 和 `Shield.h`。

更换 Shield SDK 二进制文件时，应确认原生类名、头文件接口、支持架构和最低系统版本保持兼容。

## 3. 项目结构

```text
netguard-sdk/
├─ package.json
├─ ReadMe.md
├─ ReadMe_EN.md
└─ utssdk/
   ├─ interface.uts          # 统一声明 Init
   ├─ app-android/
   │  ├─ index.uts           # Android Shield.Init 封装
   │  └─ libs/libshield.aar
   └─ app-ios/
      ├─ index.uts           # iOS Shield.getInstance().Init 封装
      └─ Libs/
         ├─ libshield.a
         └─ Shield.h
```

页面始终调用同一个 `Init(appId)`。UTS 编译器根据目标平台选择 `app-android` 或 `app-ios` 实现，业务页面不需要平台条件编译。

## 4. 与 TCP 示例的关系

NetGuard 插件只负责初始化 Shield SDK，不负责建立 TCP 连接。TCP 收发演示由独立的 `uni_modules/tcp` 插件实现，两者没有源码依赖。
