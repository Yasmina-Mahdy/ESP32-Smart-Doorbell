#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_sntp.h"
#include "mqtt_client.h"
#include <time.h>

#define PIR_GPIO GPIO_NUM_4
#define WIFI_SSID "Yasmina"
#define WIFI_PASS "esp32_test"

static const char *TAG = "PIR";

static void on_wifi_event(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data)
{
    if (event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        ESP_LOGI(TAG, "Disconnected, retrying...");
        esp_wifi_connect();
    }
}

static void on_ip_event(void *arg, esp_event_base_t event_base,
                        int32_t event_id, void *event_data)
{
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
    ESP_LOGI(TAG, "Connected! IP: " IPSTR, IP2STR(&event->ip_info.ip));
}

void wifi_init(void)
{
    nvs_flash_init();                    // non-volatile storage initialization
    esp_netif_init();                    // network interface initialization
    esp_event_loop_create_default();     // default event loop creation for background tasks
    esp_netif_create_default_wifi_sta(); // default Wi-Fi station network interface creation (as opposed to soft-AP)

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT(); // default Wi-Fi initialization configuration
    esp_wifi_init(&cfg);                                 // Wi-Fi initialization with the specified configuration

    wifi_config_t wifi_config = {
        // Wi-Fi configuration for station mode
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
        },
    };

    esp_wifi_set_mode(WIFI_MODE_STA);               // set Wi-Fi mode to station
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config); // set Wi-Fi station configuration
    esp_wifi_start();
    esp_wifi_connect();
    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &on_wifi_event, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &on_ip_event, NULL);
}

void sync_time(void)
{
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL); // Set SNTP operating mode to polling
    esp_sntp_setservername(0, "pool.ntp.org");   // Set the SNTP server name to "pool.ntp.org" for time synchronization
    esp_sntp_init();                             // Initialize the SNTP service to start time synchronization
    setenv("TZ", "UTC-3", 1);                    // Set the timezone environment variable to UTC-3 (EGYPT daylight saving time)
    tzset();                                     // Apply the timezone settings to the system

    // Wait for time to be set
    time_t now = 0;
    struct tm timeinfo = {0};

    while (timeinfo.tm_year < (2026 - 1900)) // Check if the year is less than 2026 (indicating that time has not been set)
    {
        ESP_LOGI(TAG, "Waiting for NTP sync...");
        vTaskDelay(2000 / portTICK_PERIOD_MS);
        time(&now);
        localtime_r(&now, &timeinfo);
    }

    ESP_LOGI(TAG, "Time synced!");
}

void get_timestamp(char *buffer, size_t size)
{
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    strftime(buffer, size, "%I:%M:%S %p  %d/%m/%Y", &timeinfo);
}

static esp_mqtt_client_handle_t mqtt_client;

void mqtt_init(void)
{
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = "mqtt://172.20.10.2:1883",
        .credentials.username = "esp32",
        .credentials.authentication.password = "esp32",
    };

    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_start(mqtt_client);
}

void app_main(void)
{
    wifi_init();
    sync_time();
    mqtt_init();
    // Configure GPIO4 as input
    gpio_reset_pin(PIR_GPIO);
    gpio_set_direction(PIR_GPIO, GPIO_MODE_INPUT);
    gpio_set_pull_mode(PIR_GPIO, GPIO_PULLDOWN_ONLY);

    int last_state = 0;

    while (1)
    {
        int state = gpio_get_level(PIR_GPIO);

        if (state == 1 && last_state == 0)
        {
            time_t now;
            struct tm timeinfo;
            time(&now);
            localtime_r(&now, &timeinfo);
            char timestamp[32];
            get_timestamp(timestamp, sizeof(timestamp));
            ESP_LOGI(TAG, "Motion detected! Time: %s", timestamp);
            char message[64];
            snprintf(message, sizeof(message), "Motion detected at %s", timestamp);
            esp_mqtt_client_publish(mqtt_client, "home/pir", message, 0, 1, 0);
        }

        last_state = state;
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}