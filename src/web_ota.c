#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_mac.h"
#include "nvs_flash.h"
#include "esp_http_server.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include "web_ota.h"
#include "adc_monitor.h"

static const char *TAG = "WEB_OTA";

#define WIFI_STA_SSID      "null"
#define WIFI_STA_PASS      "1234567899"
#define WIFI_STA_MAX_RETRY 10

/* ==================== 1. 前端 HTML 页面 (动态注入版本与分区信息) ==================== */
// HTML 分三段：头部, 信息面板, 主体（上传表单）
#define HTML_HEAD \
    "<!DOCTYPE html>" \
    "<html>" \
    "<head><meta charset='utf-8'><title>ESP32 OTA Update</title>" \
    "<style>body{font-family:Arial;text-align:center;margin-top:50px;} input,button{margin:10px;padding:10px;}</style></head>" \
    "<body>"

#define HTML_INFO_PLACEHOLDER "<!-- INFO_PLACEHOLDER -->"

#define HTML_BODY \
    "<h2>ESP32 Firmware Update</h2>" \
    HTML_INFO_PLACEHOLDER \
    "<p><b>Battery:</b> <span id='battery'>--</span></p>" \
    "<input type='file' id='fileInput' accept='.bin'><br>" \
    "<button onclick='upload()'>Upload & Update</button>" \
    "<p id='status'></p>" \
    "<script>" \
    "function upload() {" \
    "  var file = document.getElementById('fileInput').files[0];" \
    "  if(!file) { alert('Please select a .bin file!'); return; }" \
    "  document.getElementById('status').innerText = 'Uploading... Please wait';" \
    "  fetch('/update', {" \
    "    method: 'POST'," \
    "    body: file" \
    "  }).then(response => response.text()).then(text => {" \
    "    document.getElementById('status').innerText = text;" \
    "    if(text.includes('Success')) { setTimeout(() => location.reload(), 5000); }" \
    "  }).catch(err => {" \
    "    document.getElementById('status').innerText = 'Error: ' + err;" \
    "  });" \
    "}" \
    "function fetchBattery() {" \
    "  fetch('/api/battery')" \
    "    .then(r => r.json())" \
    "    .then(d => {" \
    "      document.getElementById('battery').innerText =" \
    "        d.voltage_v + ' V, ' + d.soc + '%';" \
    "    })" \
    "    .catch(() => {});" \
    "}" \
    "setInterval(fetchBattery, 60000);" \
    "fetchBattery();" \
    "</script>" \
    "</body>" \
    "</html>"

/* ==================== 2. HTTP 路由回调 ==================== */
// 处理 GET / 请求，返回主页（动态注入版本号与 OTA 分区名）
static esp_err_t index_get_handler(httpd_req_t *req) {
    const esp_partition_t *running = esp_ota_get_running_partition();
    const char *ota_name = running ? running->label : "unknown";

    // 动态拼接信息面板 HTML（纯文本风格，无额外 CSS）
    char info_html[512];
    snprintf(info_html, sizeof(info_html),
        "<p>"
        "<b>Version:</b> %s &nbsp;|&nbsp; "
        "<b>OTA Partition:</b> %s &nbsp;|&nbsp; "
        "<b>IDF:</b> %s &nbsp;|&nbsp; "
        "<b>Free Heap:</b> %lu KB"
        "</p>",
        WEB_OTA_VERSION,
        ota_name,
        esp_get_idf_version(),
        esp_get_free_heap_size() / 1024
    );

    // 组装完整页面：HTML_HEAD + 信息面板 + 去掉占位符的 HTML_BODY
    char *body_without_placeholder = strstr(HTML_BODY, HTML_INFO_PLACEHOLDER);
    size_t body_prefix_len = body_without_placeholder - HTML_BODY;
    size_t body_suffix_len = strlen(HTML_BODY) - body_prefix_len - strlen(HTML_INFO_PLACEHOLDER);
    const char *body_suffix = body_without_placeholder + strlen(HTML_INFO_PLACEHOLDER);

    httpd_resp_send_chunk(req, HTML_HEAD, HTTPD_RESP_USE_STRLEN);
    httpd_resp_send_chunk(req, HTML_BODY, body_prefix_len);
    httpd_resp_send_chunk(req, info_html, HTTPD_RESP_USE_STRLEN);
    httpd_resp_send_chunk(req, body_suffix, body_suffix_len);
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

// 处理 GET /api/battery 请求，返回电池 JSON
static esp_err_t battery_get_handler(httpd_req_t *req) {
    BatteryInfo info;
    if (get_battery_info(&info)) {
        char json[64];
        snprintf(json, sizeof(json),
            "{\"voltage_v\":%4.2f,\"soc\":%d}", info.voltage_v, info.soc);
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, json);
    } else {
        httpd_resp_send_500(req);
    }
    return ESP_OK;
}

// 处理 POST /update 请求，接收固件并写入 OTA 分区
static esp_err_t update_post_handler(httpd_req_t *req) {
    esp_err_t err;
    esp_ota_handle_t update_handle = 0 ;
    const esp_partition_t *update_partition = NULL;

    ESP_LOGI(TAG, "Starting OTA update...");

    // 获取下一个可用的 OTA 分区
    update_partition = esp_ota_get_next_update_partition(NULL);
    if (update_partition == NULL) {
        ESP_LOGE(TAG, "Passive OTA partition not found");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    err = esp_ota_begin(update_partition, OTA_WITH_SEQUENTIAL_WRITES, &update_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_begin failed (%s)", esp_err_to_name(err));
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    // 循环接收流式数据
    char buf[1024];
    int received = 0;
    int remaining = req->content_len;

    while (remaining > 0) {
        if ((received = httpd_req_recv(req, buf, MIN(remaining, sizeof(buf)))) <= 0) {
            if (received == HTTPD_SOCK_ERR_TIMEOUT) { continue; }
            ESP_LOGE(TAG, "File receive failed!");
            esp_ota_abort(update_handle);
            httpd_resp_send_500(req);
            return ESP_FAIL;
        }
        err = esp_ota_write(update_handle, buf, received);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "esp_ota_write failed (%s)", esp_err_to_name(err));
            esp_ota_abort(update_handle);
            httpd_resp_send_500(req);
            return ESP_FAIL;
        }
        remaining -= received;
        ESP_LOGI(TAG, "Written image length %d", req->content_len - remaining);
    }

    err = esp_ota_end(update_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_end failed (%s)", esp_err_to_name(err));
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    err = esp_ota_set_boot_partition(update_partition);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_set_boot_partition failed (%s)", esp_err_to_name(err));
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "OTA Update Success! Rebooting...");
    httpd_resp_sendstr(req, "Update Success! Rebooting device...");
    
    // 延迟 1 秒后重启，确保 HTTP 响应能够发送给前端
    vTaskDelay(pdMS_TO_TICKS(1000));
    esp_restart();
    return ESP_OK;
}

/* ==================== 3. 核心服务初始化 ==================== */
static void start_webserver(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t server = NULL;

    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_uri_t uri_get = {
            .uri      = "/",
            .method   = HTTP_GET,
            .handler  = index_get_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &uri_get);

        httpd_uri_t uri_post = {
            .uri      = "/update",
            .method   = HTTP_POST,
            .handler  = update_post_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &uri_post);

        httpd_uri_t uri_battery = {
            .uri      = "/api/battery",
            .method   = HTTP_GET,
            .handler  = battery_get_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &uri_battery);
        ESP_LOGI(TAG, "Web server started.");
    }
}

static int s_retry_num = 0;

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                                int32_t event_id, void *event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < WIFI_STA_MAX_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        } else {
            ESP_LOGE(TAG, "Failed to connect to WiFi after %d retries", WIFI_STA_MAX_RETRY);
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "WiFi Connected! IP address: " IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
    }
}

static void wifi_init_sta(void) {
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_STA_SSID,
            .password = WIFI_STA_PASS,
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    esp_err_t pwr_err = esp_wifi_set_max_tx_power(40);
    if (pwr_err == ESP_OK) {
        ESP_LOGI(TAG, "WiFi TX power successfully limited to 10dBm to prevent BOD.");
    } else {
        ESP_LOGE(TAG, "Failed to limit WiFi TX power: %s", esp_err_to_name(pwr_err));
    }

    ESP_LOGI(TAG, "WiFi STA mode started. Connecting to SSID:%s", WIFI_STA_SSID);
}

void start_web_ota(void) {
    // 确保 NVS 已经由 main 函数初始化
    wifi_init_sta();
    start_webserver();
}