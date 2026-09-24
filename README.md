# STOP Wireless

本仓库采用单仓库多子项目（monorepo）方式管理无线停止系统的固件、网页端和设计资料。

## 目录说明

```text
STOP_wireless/
├─ STOP/       PlatformIO 嵌入式固件
├─ web/        Web 前端
└─ Assused/    硬件设计和辅助资料
```

## 固件项目

使用 VS Code 打开本仓库后，通过 PlatformIO 打开或编译 `STOP/platformio.ini`。

也可以在终端中运行：

```powershell
Set-Location .\STOP
platformio run
```

## Web 项目

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
