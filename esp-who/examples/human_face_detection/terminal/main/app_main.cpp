#include "who_camera.h"
#include "who_human_face_detection.hpp"
#include "esp_http_server.h"
#include "esp_log.h"
#include "img_converters.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_http_client.h"
#include "nvs_flash.h"

#define WIFI_SSID "Ronza_EXT"
#define WIFI_PASS "Smsm@@smsm@69"
#define WIFI_CONNECTED_BIT BIT0

static EventGroupHandle_t wifi_event_group;
static QueueHandle_t xQueueAIFrame = NULL;
static QueueHandle_t xQueueResult  = NULL;
static QueueHandle_t xQueueFrame   = NULL;

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                                int32_t event_id, void *event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
        esp_wifi_connect();
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
}

static void wifi_init(void) {
    wifi_event_group = xEventGroupCreate();
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL);
    wifi_config_t wifi_config = {};
    strcpy((char*)wifi_config.sta.ssid, WIFI_SSID);
    strcpy((char*)wifi_config.sta.password, WIFI_PASS);
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    esp_wifi_start();
    esp_wifi_connect();
    xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_BIT, false, true, portMAX_DELAY);
    ESP_LOGI("wifi", "Connected!");
}

static esp_err_t jpg_handler(httpd_req_t *req) {
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) return ESP_FAIL;
    uint8_t *jpeg_buf = NULL;
    size_t jpeg_len = 0;
    if (frame2jpg(fb, 80, &jpeg_buf, &jpeg_len)) {
        httpd_resp_set_type(req, "image/jpeg");
        httpd_resp_send(req, (char*)jpeg_buf, jpeg_len);
        free(jpeg_buf);
    }
    esp_camera_fb_return(fb);
    return ESP_OK;
}

static void start_webserver(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t server = NULL;
    httpd_start(&server, &config);
    httpd_uri_t uri = {
        .uri     = "/",
        .method  = HTTP_GET,
        .handler = jpg_handler,
    };
    httpd_register_uri_handler(server, &uri);
    ESP_LOGI("web", "Open http://192.168.1.57 in browser");
}

static void http_post_data(const uint8_t *data, size_t data_len) {
    char url[64];
    snprintf(url, sizeof(url), "http://192.168.1.62:5000/upload/image");

    esp_http_client_config_t config = {};
    config.url = url;
    config.method = HTTP_METHOD_POST;
    config.timeout_ms = 10000;

    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_http_client_set_header(client, "Content-Type", "application/octet-stream");
    esp_http_client_set_post_field(client, (const char *)data, data_len);

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK)
        ESP_LOGI("http", "Image uploaded! status: %d", esp_http_client_get_status_code(client));
    else
        ESP_LOGE("http", "Upload failed: %s", esp_err_to_name(err));

    esp_http_client_cleanup(client);
}

static void detection_bridge_task(void *arg) {
    bool detected;
    while (true) {
        if (xQueueReceive(xQueueResult, &detected, portMAX_DELAY)) {
            if (detected) {
                camera_fb_t *fb = esp_camera_fb_get();
                if (fb) {
                    if (xQueueSend(xQueueFrame, &fb, 0) != pdTRUE) {
                        esp_camera_fb_return(fb);
                    }
                }
            }
        }
    }
}

static void upload_task(void *arg) {
    camera_fb_t *fb = NULL;
    while (true) {
        if (xQueueReceive(xQueueFrame, &fb, portMAX_DELAY)) {
            if (fb) {
                ESP_LOGI("doorbell", ">>> FACE DETECTED! Uploading saved frame...");
                uint8_t *jpeg_buf = NULL;
                size_t jpeg_len = 0;
                if (frame2jpg(fb, 80, &jpeg_buf, &jpeg_len)) {
                    http_post_data(jpeg_buf, jpeg_len);
                    free(jpeg_buf);
                }
                esp_camera_fb_return(fb);
                vTaskDelay(pdMS_TO_TICKS(5000));
            }
        }
    }
}

extern "C" void app_main() {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    esp_log_level_set("human_face_detection", ESP_LOG_NONE);
    wifi_init();

    xQueueAIFrame = xQueueCreate(2, sizeof(camera_fb_t *));
    xQueueResult  = xQueueCreate(2, sizeof(bool));
    xQueueFrame   = xQueueCreate(1, sizeof(camera_fb_t *));

    register_camera(PIXFORMAT_RGB565, FRAMESIZE_240X240, 2, xQueueAIFrame);
    register_human_face_detection(xQueueAIFrame, NULL, xQueueResult, NULL, true);

    start_webserver();
    xTaskCreatePinnedToCore(detection_bridge_task, "bridge", 4 * 1024, NULL, 5, NULL, 1);
    xTaskCreatePinnedToCore(upload_task, "upload", 16 * 1024, NULL, 4, NULL, 1);
}
