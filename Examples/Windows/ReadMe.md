# NetGuard Windows 原生演示项目

[English](./ReadMe_EN.md)

本项目使用纯 Win32 C 演示如何初始化 NetGuard Shield SDK，以及如何通过本机 TCP 连接验证 NetGuard 启动的服务。

## 构建

本项目使用 **GCC for Windows** 编译，可以从 WinLibs 下载：

https://winlibs.com/

Makefile 假设 32 位 GCC for Windows 已解压到：

```text
D:\MinGW32\
```

因此默认使用：

```text
D:\MinGW32\bin\gcc.exe
D:\MinGW32\bin\windres.exe
D:\MinGW32\bin\mingw32-make.exe
```

构建命令：

```powershell
cd Examples\Windows
D:\MinGW32\bin\mingw32-make.exe
```

如果 GCC for Windows 不在 `D:\MinGW32\`，请修改 `Makefile` 开头的这两个变量：

```makefile
CC := D:/MinGW32/bin/gcc.exe
WINDRES := D:/MinGW32/bin/windres.exe
```

将它们替换为实际的 `gcc.exe` 和 `windres.exe` 路径，然后使用实际路径下的 `mingw32-make.exe` 执行构建。Makefile 中使用正斜杠 `/` 书写路径。

生成文件为：

```text
Release/WinDemo.exe
```

运行时请确保 `WinDemo.exe` 与 `Shield.dll` 位于同一个 `Release` 目录。

## 功能

- 通过 `LoadLibrary` 加载 `Shield.dll` 并调用导出的 `Init`。
- 将 `res/Shield.ico` 作为窗口和可执行文件图标。
- 可编辑 Host 和 Port，默认连接 `127.69.88.11:10000`。
- 显示 SDK 初始化、连接状态和通信日志。
- 每秒发送一个 ping，并持续接收服务端响应。
- 分类显示最近 200 条日志。
- 支持 5 秒连接超时、部分发送、TCP 拆包和粘包。
- 在连接、协议或发送错误后统一释放 Socket 和工作线程。

## 代码结构

```text
Windows/
├─ Makefile
├─ ReadMe.md
├─ ReadMe_EN.md
├─ include/
│  ├─ shield_sdk.h
│  └─ tcp_client.h
├─ src/
│  ├─ main.c          # Win32 界面、日志和应用生命周期
│  ├─ shield_sdk.c    # Shield.dll 动态加载
│  └─ tcp_client.c    # Winsock TCP 客户端
├─ res/
│  ├─ Shield.ico
│  └─ resource.rc
└─ Release/
   ├─ Shield.dll
   └─ WinDemo.exe
```

## TCP 协议

每个数据帧由 4 字节小端序 payload 长度和 UTF-8 payload 组成，完整帧最大为 16 KiB。连接成功后客户端每秒发送：

```json
{"cmd":"ping","id":0,"time":"12:34:56.789"}
```
