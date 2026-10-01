"""编译实际传输层和配置 C 代码，用可控 SDK 替身验证故障路径；不代替射频实测。"""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
CC = os.environ.get("HOST_CC") or shutil.which("gcc") or r"C:\Program Files (x86)\Dev-Cpp\MinGW64\bin\gcc.exe"

SDK = r'''
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_NO_MEM 1
#define ESP_ERR_INVALID_ARG 2
#define ESP_ERR_INVALID_STATE 3
#define ESP_ERR_TIMEOUT 4
#define ESP_ERR_INVALID_SIZE 5
#define ESP_ERR_NOT_SUPPORTED 6
#define ESP_ERR_NVS_NOT_FOUND 7
#define ESP_ERR_INVALID_CRC 8
static inline const char *esp_err_to_name(int e) { (void)e; return "mock"; }
#define ESP_LOGI(tag, ...) do { (void)(tag); } while(0)
#define ESP_LOGW(tag, ...) do { (void)(tag); } while(0)
typedef uint32_t TickType_t;
#define portMAX_DELAY UINT32_MAX
#define pdMS_TO_TICKS(x) (x)
#define pdTRUE 1
#define pdPASS 1
static TickType_t ticks;
static inline TickType_t xTaskGetTickCount(void) { return ticks; }
static inline void vTaskDelay(TickType_t n) { ticks += n; }
static inline int xTaskCreate(void (*fn)(void*), const char *n, unsigned st, void *a, int p, void *h)
{ (void)fn;(void)n;(void)st;(void)a;(void)p;(void)h; return pdPASS; }
typedef struct { int token; } *SemaphoreHandle_t;
static inline SemaphoreHandle_t xSemaphoreCreateMutex(void) {
    SemaphoreHandle_t s = calloc(1, sizeof(*s)); s->token = 1; return s;
}
static inline SemaphoreHandle_t xSemaphoreCreateBinary(void) {
    return calloc(1, sizeof(struct {int token;}));
}
static inline int xSemaphoreTake(SemaphoreHandle_t s, TickType_t t) {
    if (s && s->token) { s->token = 0; return pdTRUE; } ticks += t; return 0;
}
static inline void xSemaphoreGive(SemaphoreHandle_t s) { s->token = 1; }
static inline void vSemaphoreDelete(SemaphoreHandle_t s) { free(s); }
typedef struct { size_t size; unsigned capacity, count; unsigned char data[8192]; } *QueueHandle_t;
static inline QueueHandle_t xQueueCreate(unsigned n, size_t size) {
    QueueHandle_t q = calloc(1, sizeof(*q)); q->size = size; q->capacity = n; return q;
}
static inline int xQueueSend(QueueHandle_t q, const void *v, TickType_t t) {
    (void)t; if (q->count == q->capacity) return 0;
    memcpy(q->data + q->count++ * q->size, v, q->size); return pdTRUE;
}
static inline int xQueueOverwrite(QueueHandle_t q, const void *v) {
    assert(q->capacity == 1); memcpy(q->data, v, q->size); q->count = 1; return pdTRUE;
}
static inline int xQueueReceive(QueueHandle_t q, void *v, TickType_t t) {
    if (!q->count) { ticks += t; return 0; }
    memcpy(v, q->data, q->size); --q->count;
    memmove(q->data, q->data + q->size, q->count * q->size); return pdTRUE;
}
static inline void vQueueDelete(QueueHandle_t q) { free(q); }
#define GPIO_NUM_2 2
#define GPIO_NUM_3 3
#define GPIO_MODE_OUTPUT 1
static inline int gpio_set_level(int p, int l) { (void)p;(void)l;return 0; }
static inline int gpio_set_direction(int p, int d) { (void)p;(void)d;return 0; }
#define ESP_IDF_VERSION_VAL(a,b,c) ((a)*10000+(b)*100+(c))
#define ESP_IDF_VERSION ESP_IDF_VERSION_VAL(5,5,3)
typedef struct {int unused;} wifi_init_config_t;
#define WIFI_INIT_CONFIG_DEFAULT() ((wifi_init_config_t){0})
#define WIFI_STORAGE_RAM 0
#define WIFI_MODE_STA 1
#define WIFI_PS_NONE 0
#define WIFI_SECOND_CHAN_NONE 0
#define WIFI_IF_STA 0
static int wifi_error, now_error, now_sends, auto_complete = 1;
static unsigned actual_channel;
static inline int esp_netif_init(void) { return 0; }
static inline int esp_event_loop_create_default(void) { return 0; }
static inline int esp_wifi_init(const wifi_init_config_t *c) { (void)c; return wifi_error; }
static inline int esp_wifi_set_storage(int v) { (void)v;return 0; }
static inline int esp_wifi_set_mode(int v) { (void)v;return 0; }
static inline int esp_wifi_set_ps(int v) { (void)v;return 0; }
static inline int esp_wifi_start(void) { return 0; }
static inline int esp_wifi_stop(void) { return 0; }
static inline int esp_wifi_deinit(void) { return 0; }
static inline int esp_wifi_set_channel(unsigned c, int s) { (void)s; actual_channel=c; return 0; }
typedef struct {int unused;} esp_now_recv_info_t;
typedef struct {int unused;} esp_now_send_info_t;
typedef int esp_now_send_status_t;
#define ESP_NOW_SEND_SUCCESS 0
#define ESP_NOW_SEND_FAIL 1
typedef struct { uint8_t peer_addr[6]; unsigned channel; int ifidx; bool encrypt; } esp_now_peer_info_t;
static void (*tx_cb)(const esp_now_send_info_t*, esp_now_send_status_t);
static inline int esp_now_init(void) { return now_error; }
static inline int esp_now_deinit(void) { return 0; }
static inline int esp_now_register_recv_cb(void (*cb)(const esp_now_recv_info_t*,const uint8_t*,int)) { (void)cb;return 0; }
static inline int esp_now_register_send_cb(void (*cb)(const esp_now_send_info_t*,esp_now_send_status_t)) { tx_cb=cb;return 0; }
static inline int esp_now_add_peer(const esp_now_peer_info_t *p) { assert(p->channel==actual_channel);return 0; }
static inline int esp_now_send(const uint8_t *p,const uint8_t *d,size_t n) {
    (void)p;(void)d;(void)n; ++now_sends;
    if (auto_complete) tx_cb(NULL, ESP_NOW_SEND_SUCCESS);
    return 0;
}
typedef int nvs_handle_t;
#define NVS_READONLY 0
#define NVS_READWRITE 1
static uint32_t stored_mode, stored_channel = 6;
static bool stored, fail_commit;
static inline int nvs_open(const char *ns,int mode,nvs_handle_t *h) {
    (void)ns;*h=1;return !stored && mode==NVS_READONLY ? ESP_ERR_NVS_NOT_FOUND : ESP_OK;
}
static inline void nvs_close(nvs_handle_t h) { (void)h; }
static inline int nvs_get_str(nvs_handle_t h,const char *k,char *v,size_t *n) { (void)h;(void)k;(void)v;(void)n;return ESP_ERR_NVS_NOT_FOUND; }
static inline int nvs_get_u32(nvs_handle_t h,const char *k,uint32_t *v) {
    (void)h; if (!strcmp(k,"radio_link")) *v=stored_mode;
    else if (!strcmp(k,"now_channel")) *v=stored_channel;
    else return ESP_ERR_NVS_NOT_FOUND; return 0;
}
static inline int nvs_set_str(nvs_handle_t h,const char *k,const char *v) { (void)h;(void)k;(void)v;return 0; }
static inline int nvs_set_u32(nvs_handle_t h,const char *k,uint32_t v) {
    (void)h; if (!strcmp(k,"radio_link")) stored_mode=v;
    if (!strcmp(k,"now_channel")) stored_channel=v; return 0;
}
static inline int nvs_commit(nvs_handle_t h) { (void)h;stored=true;return fail_commit ? ESP_FAIL : ESP_OK; }
'''

TRANSPORT_TEST = r'''
#include "radio_transport.c"
static device_config_t config;
static int e22_error, e22_sends;
static bool have_e22_packet;
esp_err_t device_config_init(void) { return 0; }
void device_config_get(device_config_t *out) { *out=config; }
esp_err_t e22_init(void) { return e22_error; }
esp_err_t e22_send(const uint8_t *d,size_t n,TickType_t t) { (void)d;(void)n;(void)t;++e22_sends;return e22_error; }
esp_err_t e22_receive(e22_rx_msg_t *m,TickType_t t) { (void)t;if (!have_e22_packet)return ESP_ERR_TIMEOUT;have_e22_packet=false;m->len=1;m->data[0]=99;return 0; }
e22_mode_t e22_get_mode(void) { return E22_MODE_TRANSMIT; }
int main(int argc,char **argv) {
    assert(argc==2); int scenario=atoi(argv[1]);
    config.radio_link=scenario==0 ? 0 : scenario>=3 ? 2 : 1;
    config.espnow_channel=11;
    if (scenario==3 || scenario==5) e22_error=ESP_FAIL;
    if (scenario==4 || scenario==5) wifi_error=ESP_FAIL;
    int init=radio_transport_init();
    if (scenario==5) { assert(init!=ESP_OK);return 0; }
    assert(init==ESP_OK);
    uint8_t data[3]={1,2,3}; radio_rx_msg_t rx;
    assert(radio_transport_send(NULL,3,10)==ESP_ERR_INVALID_ARG);
    assert(radio_transport_send(data,236,10)==ESP_ERR_INVALID_ARG);
    if (scenario==0) {
        assert(!s_ready && radio_transport_mode()==E22_MODE_TRANSMIT);
        assert(radio_transport_send(data,3,10)==ESP_OK && e22_sends==1 && now_sends==0);
        have_e22_packet=true;assert(radio_transport_receive(&rx,0)==ESP_OK && rx.data[0]==99);
    } else if (scenario==1) {
        assert(actual_channel==11 && !s_e22_ready && radio_transport_mode()==5);
        esp_now_recv_info_t info={0};
        receive_callback(&info,data,3);
        assert(radio_transport_receive(&rx,0)==ESP_OK && rx.len==3 && !memcmp(rx.data,data,3));
        receive_callback(&info,data,3);ticks+=201;
        assert(radio_transport_receive(&rx,0)==ESP_ERR_TIMEOUT); /* 过期心跳不得复活链路 */
        receive_callback(&info,data,236);
        assert(radio_transport_receive(&rx,0)==ESP_ERR_TIMEOUT);
    } else if (scenario==2) {
        auto_complete=0;
        assert(radio_transport_send(data,3,10)==ESP_ERR_TIMEOUT && now_sends==1);
        assert(radio_transport_send(data,3,10)==ESP_ERR_TIMEOUT && now_sends==1);
        tx_cb(NULL,ESP_NOW_SEND_SUCCESS); /* 第一帧迟到的完成回调 */
        assert(radio_transport_send(data,3,10)==ESP_ERR_TIMEOUT && now_sends==2);
        tx_cb(NULL,ESP_NOW_SEND_FAIL);auto_complete=1;
        assert(radio_transport_send(data,3,10)==ESP_OK && now_sends==3);
    } else {
        assert(radio_transport_mode()==6);
        assert(radio_transport_send(data,3,10)==ESP_OK); /* 任一路可用即可降级运行 */
        if (s_e22_ready) {
            data[0]=0;assert(radio_transport_send(data,3,10)==ESP_OK);
            e22_job_t job;assert(xQueueReceive(s_e22_tx_queue,&job,0)==pdTRUE && job.msg.data[0]==0);
            assert(xQueueReceive(s_e22_tx_queue,&job,0)!=pdTRUE); /* 只保留最新发送 */
            have_e22_packet=true;assert(radio_transport_receive(&rx,0)==ESP_OK);
        }
    }
    return 0;
}
'''

CONFIG_TEST = r'''
#include "device_config.c"
uint16_t stop_read_le16(const uint8_t *p) { return p[0] | (p[1]<<8); }
uint32_t stop_read_le32(const uint8_t *p) { return p[0] | (p[1]<<8) | ((uint32_t)p[2]<<16) | ((uint32_t)p[3]<<24); }
void stop_write_le16(uint8_t *p,uint16_t v) { p[0]=v;p[1]=v>>8; }
void stop_write_le32(uint8_t *p,uint32_t v) { for(int i=0;i<4;++i)p[i]=v>>(i*8); }
static int apply(unsigned mode,unsigned channel) {
    uint8_t p[]={2,4,0,2,4,0,0,0,0,0,5,0,2,4,0,0,0,0,0};
    stop_write_le32(p+6,mode);stop_write_le32(p+15,channel);
    return device_config_apply_tlv(p,sizeof(p));
}
int main(void) {
    assert(device_config_init()==ESP_OK);
    device_config_t c;device_config_get(&c);assert(c.radio_link==0 && c.espnow_channel==6);
    for(unsigned m=0;m<3;++m) for(unsigned ch=1;ch<=11;++ch) {
        assert(apply(m,ch)==ESP_OK);device_config_get(&c);
        assert(c.radio_link==m && c.espnow_channel==ch);
    }
    assert(apply(3,6)==ESP_ERR_INVALID_ARG);
    assert(apply(1,0)==ESP_ERR_INVALID_ARG);
    assert(apply(1,12)==ESP_ERR_INVALID_ARG);
    device_config_get(&c);assert(c.radio_link==2 && c.espnow_channel==11);
    uint8_t legacy[]={1,3,0,2,4,0,0xe8,3,0,0};
    assert(device_config_apply_tlv(legacy,sizeof(legacy))==ESP_OK);
    device_config_get(&c);assert(c.radio_link==2 && c.espnow_channel==11);
    uint8_t out[128];size_t n;
    assert(device_config_encode_tlv(NULL,0,out,sizeof(out),&n)==ESP_OK && out[0]==5);
    uint8_t keys[]={4,0,5,0};
    assert(device_config_encode_tlv(keys,2,out,sizeof(out),&n)==ESP_OK && n==19);
    assert(stop_read_le32(out+6)==2 && stop_read_le32(out+15)==11);
    assert(device_config_encode_tlv(keys,2,out,18,&n)==ESP_ERR_INVALID_SIZE);
    vSemaphoreDelete(s_mutex);s_mutex=NULL;assert(device_config_init()==ESP_OK);
    device_config_get(&c);assert(c.radio_link==2 && c.espnow_channel==11); /* 模拟重启读取 NVS */
    fail_commit=true;assert(apply(1,6)==ESP_FAIL);
    device_config_get(&c);assert(c.radio_link==2 && c.espnow_channel==11);
    return 0;
}
'''


class RadioHostTests(unittest.TestCase):
    def test_modes_timeouts_and_config(self):
        with tempfile.TemporaryDirectory(prefix="stop_radio_") as temp:
            folder = Path(temp)
            (folder / "sdk.h").write_text(SDK, encoding="utf-8")
            headers = ["esp_err.h", "esp_event.h", "esp_idf_version.h", "esp_log.h", "esp_netif.h",
                       "esp_now.h", "esp_wifi.h", "nvs.h", "driver/gpio.h", "driver/uart.h",
                       "freertos/FreeRTOS.h", "freertos/semphr.h", "freertos/queue.h", "freertos/task.h"]
            for header in headers:
                path = folder / header
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text('#include "sdk.h"\n', encoding="utf-8")
            for project in ("STOP", "Assused"):
                for name, body in (("transport", TRANSPORT_TEST), ("config", CONFIG_TEST)):
                    source = folder / f"{project}_{name}.c"
                    source.write_text(body, encoding="utf-8")
                    exe = source.with_suffix(".exe")
                    subprocess.run([CC, "-std=gnu99", "-I", str(folder), "-I", str(ROOT / project / "src"),
                                    str(source), "-o", str(exe)], check=True)
                    for scenario in (range(7) if name == "transport" else [None]):
                        with self.subTest(project=project, module=name, scenario=scenario):
                            subprocess.run([str(exe)] + ([] if scenario is None else [str(scenario)]), check=True)


if __name__ == "__main__":
    unittest.main(verbosity=2)
