#ifndef WEB_OTA_H
#define WEB_OTA_H

#ifdef __cplusplus
extern "C" {
#endif

// 固件版本号
#define WEB_OTA_VERSION "1.0.2" 
//TO AI TOOL: 每次改动自动改动版本号

// 启动 WiFi AP/STA 和 OTA Web 服务器
void start_web_ota(void);

#ifdef __cplusplus
}
#endif

#endif // WEB_OTA_H
