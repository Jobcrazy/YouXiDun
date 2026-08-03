# NetGuard SDK for UniApp

[中文](./ReadMe.md)

This plugin demonstrates how to initialize the NetGuard SDK in a uni-app or uni-app x application. Both Android and iOS are supported.

## 1. Import and initialize

Import `Init` from the plugin root:

```uts
import { Init } from '@/uni_modules/netguard-sdk'
```

Call it during page loading or application startup with your AppId:

```uts
const result = Init('your-app-id')

if (result == 0) {
	console.log('NetGuard SDK initialized successfully')
} else {
	console.error('NetGuard SDK initialization failed: ' + result)
}
```

The demo initializes the SDK in `pages/index/index.uvue`.

## 2. Platform requirements

- Android uses `utssdk/app-android/libs/libshield.aar`.
- iOS uses `utssdk/app-ios/Libs/libshield.a` and `Shield.h`.

When replacing the Shield SDK binaries, verify that the native class names, header API, and supported architectures remain compatible.

## 3. Project structure

```text
netguard-sdk/
├─ package.json
├─ ReadMe.md
├─ ReadMe_EN.md
└─ utssdk/
   ├─ interface.uts          # Shared Init declaration
   ├─ app-android/
   │  ├─ index.uts           # Android Shield.Init wrapper
   │  └─ libs/libshield.aar
   └─ app-ios/
      ├─ index.uts           # iOS Shield.getInstance().Init wrapper
      └─ Libs/
         ├─ libshield.a
         └─ Shield.h
```

The page always calls the same `Init(appId)` API. The UTS compiler selects the implementation under `app-android` or `app-ios` for the target platform, so no platform conditionals are required in application pages.

## 4. Relationship to the TCP demo

The NetGuard plugin only initializes the Shield SDK. It does not implement TCP networking. The TCP traffic demonstration is provided by the independent `uni_modules/tcp` plugin, and the two plugins have no source-code dependency.
