# STOP-C6 Web 工具箱

Vue 3 + TypeScript + Vite 的 Web Bluetooth PWA。浏览器直接连接 ESP32-C6，云端只负责托管静态页面和固件文件。

## 本地开发

```powershell
cd web
npm install
npm run dev
```

`localhost` 可使用 Web Bluetooth。部署环境必须使用 HTTPS。推荐 Chrome 或 Edge，并确保电脑/Android 设备具有 BLE 功能。

## 云端固件

固件 URL 必须允许网页来源跨域读取（CORS），内容必须为原始 `.bin`，不能返回下载 HTML 页面。界面会在浏览器内计算 SHA-256，然后按 `Document/BLE_PROTOCOL.md` 分块发送。

## 安全边界

当前网页和 C6 尚未实现维护身份认证与量产固件签名。不要将本工具部署到不可信环境，也不要将当前 OTA 能力用于正式安全产品。
