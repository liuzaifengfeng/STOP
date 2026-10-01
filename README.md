# STOP Wireless

本仓库采用单仓库多子项目（monorepo）方式管理无线停止系统的固件、网页端和设计资料。

## 目录说明

```text
STOP_wireless/
├─ STOP/       控制端 PlatformIO 固件
├─ web/        Web 前端
└─ Assused/    被控端 Beta PlatformIO 固件与硬件资料
```

## 固件项目

使用 VS Code 打开本仓库后，通过 PlatformIO 分别打开或编译控制端 `STOP/platformio.ini`、被控端 `Assused/platformio.ini`。被控端引脚、配对与上电步骤见 [Assused/README.md](Assused/README.md)。

也可以在终端中运行：

```powershell
Set-Location .\STOP
platformio run
```

## Web 项目

固件现支持在网页“设备配置”选择 **E22、ESP-NOW 或双通道**，模式与 ESP-NOW 信道保存后重启生效。使用步骤、双通道语义和实测项目见 [ESP-NOW 说明](STOP/Document/ESPNOW.md)。

首次使用时安装依赖：

```powershell
Set-Location .\web
npm install
npm run dev
```

生成发布版本：

```powershell
npm run build
```

## Git 管理

Git 根目录位于 `STOP_wireless`。固件、网页端和设计资料统一提交到同一个 GitHub 仓库；`.pio`、`node_modules`、`dist` 等自动生成内容不会进入版本控制。
