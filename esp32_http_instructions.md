# ESP32 HTTP Upload Integration Guide
## For Camera and Audio teammates

---

## Overview

The Flask server accepts two HTTP POST endpoints:

| Endpoint | What to send |
|---|---|
| `POST /upload/image` | Raw JPEG bytes from camera |
| `POST /upload/audio` | Raw audio bytes from microphone |

The ESP32 sends a raw POST request with the file bytes as the body. That's it — no multipart, no JSON, just raw bytes.

---

## Step 1 — Project Setup

In your `CMakeLists.txt`, make sure these components are included:

```cmake
idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES esp_wifi nvs_flash esp_http_client esp_sntp
)
```

---

## Step 2 — WiFi Connection

Add this to your `main.c`. This connects to the hotspot and blocks until connected.

```c
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"

#define WIFI_SSID      "your_hotspot_name"
#define WIFI_PASS      "your_hotspot_password"
#define WIFI_CONNECTED_BIT BIT0

static EventGroupHandle_t wifi_event_group;
static const char *TAG = "wifi";

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                                int32_t event_id, void *event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

void wifi_init(void) {
    wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    esp_wifi_connect();

    // Wait until connected
    xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_BIT, false, true, portMAX_DELAY);
    ESP_LOGI(TAG, "WiFi connected");
}
```

---

## Step 3 — NTP Time Sync

This sets the ESP32 clock to Cairo time. Call this after WiFi connects.

```c
#include "esp_sntp.h"
#include "time.h"

void time_sync_init(void) {
    setenv("TZ", "EET-2EEST,M3.5.4/0,M10.5.5/1", 1);
    tzset();
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_init();

    // Wait for time to sync
    time_t now = 0;
    struct tm timeinfo = {0};
    while (timeinfo.tm_year < (2020 - 1900)) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        time(&now);
        localtime_r(&now, &timeinfo);
    }
}

// Call this to get a formatted timestamp string
// buf must be at least 30 chars
void get_timestamp(char *buf) {
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    strftime(buf, 30, "%I:%M:%S %p %d/%m/%Y", &timeinfo);
}
```

---

## Step 4 — HTTP POST Function

This is the function that actually sends data to the server. Use it for both image and audio.

```c
#include "esp_http_client.h"

#define SERVER_IP   "192.168.x.x"   // <-- Replace with the PC's IP on the hotspot
#define SERVER_PORT 5000

// endpoint: either "/upload/image" or "/upload/audio"
// data: pointer to your byte buffer (JPEG or audio bytes)
// data_len: size of the buffer in bytes
void http_post_data(const char *endpoint, const uint8_t *data, size_t data_len) {
    char timestamp[30];
    get_timestamp(timestamp);

    // Build full URL with timestamp as query param
    char url[128];
    snprintf(url, sizeof(url), "http://%s:%d%s?timestamp=%s",
             SERVER_IP, SERVER_PORT, endpoint, timestamp);

    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_POST,
        .timeout_ms = 10000,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_http_client_set_header(client, "Content-Type", "application/octet-stream");
    esp_http_client_set_post_field(client, (const char *)data, data_len);

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        ESP_LOGI("http", "POST to %s OK, status: %d",
                 endpoint, esp_http_client_get_status_code(client));
    } else {
        ESP_LOGE("http", "POST failed: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
}
```

---

## Step 5 — Calling It

### For the camera teammate (send JPEG):

```c
// Assuming jpeg_buf is your captured frame buffer and jpeg_len is its size
http_post_data("/upload/image", jpeg_buf, jpeg_len);
```

### For the audio teammate (send raw audio):

```c
// Assuming audio_buf is your recorded samples and audio_len is its size in bytes
http_post_data("/upload/audio", audio_buf, audio_len);
```

---

## Step 6 — app_main

```c
void app_main(void) {
    // Init NVS (required for WiFi)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Connect to WiFi
    wifi_init();

    // Sync time
    time_sync_init();

    // Your capture/record logic goes here
    // Then call http_post_data() when you have data ready
}
```

---

## Key Notes

- **SERVER_IP**: Must be the PC's IP on the hotspot (same one used in `mqtt_client.py`). Not `localhost`.
- **Timestamp**: The `?timestamp=` query param is optional — if omitted, the server generates one automatically.
- **Audio format**: Send whatever bytes your I2S/microphone driver produces. Coordinate with the PIR teammate on what format the server expects.
- **Image format**: OV2640 outputs JPEG directly — pass the frame buffer pointer and length straight to `http_post_data`.
- **No response parsing needed**: The server returns `{"status": "ok"}` on success. You can ignore it or log the status code.
