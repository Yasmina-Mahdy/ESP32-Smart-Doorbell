#include "who_camera.h"
#include "esp_log.h"
#include "driver/i2c_master.h"

static const char *TAG = "who_camera";
static QueueHandle_t xQueueFrameO = NULL;

static void axp313a_power_on(void)
{
    // AXP313A is on pins 1(SDA) and 2(SCL)
    i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = 1,
        .scl_io_num = 2,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    i2c_master_bus_handle_t bus_handle;
    i2c_new_master_bus(&bus_config, &bus_handle);

    // Scan to confirm AXP313A is visible
    ESP_LOGI(TAG, "Scanning I2C bus...");
    for (uint8_t addr = 0x08; addr < 0x78; addr++) {
        if (i2c_master_probe(bus_handle, addr, 10) == ESP_OK)
            ESP_LOGI(TAG, "  Found device at 0x%02x", addr);
    }

    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = 0x36,
        .scl_speed_hz = 100000,
    };
    i2c_master_dev_handle_t dev_handle;
    i2c_master_bus_add_device(bus_handle, &dev_config, &dev_handle);

    // Init AXP313A
    uint8_t d0[] = {0x00, 0x04};
    i2c_master_transmit(dev_handle, d0, 2, 100);
    vTaskDelay(pdMS_TO_TICKS(10));
    // Enable ALDO and DLDO outputs
    uint8_t d1[] = {0x10, 0x19};
    i2c_master_transmit(dev_handle, d1, 2, 100);
    vTaskDelay(pdMS_TO_TICKS(10));
    // DVDD = 1.2V
    uint8_t d2[] = {0x16, 0x07};
    i2c_master_transmit(dev_handle, d2, 2, 100);
    vTaskDelay(pdMS_TO_TICKS(10));
    // AVDD/DOVDD = 2.8V
    uint8_t d3[] = {0x17, 0x17};
    i2c_master_transmit(dev_handle, d3, 2, 100);
    vTaskDelay(pdMS_TO_TICKS(10));

    i2c_master_bus_rm_device(dev_handle);
    i2c_del_master_bus(bus_handle);

    // Wait for camera power rails to stabilize
    vTaskDelay(pdMS_TO_TICKS(500));
    ESP_LOGI(TAG, "AXP313A camera power enabled");

    // Scan again after power on - OV2640 should appear at 0x30
    i2c_master_bus_handle_t scan_handle;
    if (i2c_new_master_bus(&bus_config, &scan_handle) == ESP_OK) {
        ESP_LOGI(TAG, "Post-power scan:");
        for (uint8_t addr = 0x08; addr < 0x78; addr++) {
            if (i2c_master_probe(scan_handle, addr, 10) == ESP_OK)
                ESP_LOGI(TAG, "  Found: 0x%02x", addr);
        }
        i2c_del_master_bus(scan_handle);
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

static void task_process_handler(void *arg)
{
    while (true)
    {
        camera_fb_t *frame = esp_camera_fb_get();
        if (frame) {
            if (frame->len > 0)
                xQueueSend(xQueueFrameO, &frame, portMAX_DELAY);
            else
                esp_camera_fb_return(frame);
        }
    }
}

void register_camera(const pixformat_t pixel_fromat,
                     const framesize_t frame_size,
                     const uint8_t fb_count,
                     const QueueHandle_t frame_o)
{
    axp313a_power_on();

    ESP_LOGI(TAG, "Camera module is %s", CAMERA_MODULE_NAME);

    camera_config_t config = {0};
    config.ledc_channel  = LEDC_CHANNEL_0;
    config.ledc_timer    = LEDC_TIMER_0;
    config.pin_d0        = CAMERA_PIN_D0;
    config.pin_d1        = CAMERA_PIN_D1;
    config.pin_d2        = CAMERA_PIN_D2;
    config.pin_d3        = CAMERA_PIN_D3;
    config.pin_d4        = CAMERA_PIN_D4;
    config.pin_d5        = CAMERA_PIN_D5;
    config.pin_d6        = CAMERA_PIN_D6;
    config.pin_d7        = CAMERA_PIN_D7;
    config.pin_xclk      = CAMERA_PIN_XCLK;
    config.pin_pclk      = CAMERA_PIN_PCLK;
    config.pin_vsync     = CAMERA_PIN_VSYNC;
    config.pin_href      = CAMERA_PIN_HREF;
    config.pin_sccb_sda  = CAMERA_PIN_SIOD;
    config.pin_sccb_scl  = CAMERA_PIN_SIOC;
    config.pin_pwdn      = CAMERA_PIN_PWDN;
    config.pin_reset     = CAMERA_PIN_RESET;
    config.xclk_freq_hz  = XCLK_FREQ_HZ;
    config.pixel_format  = pixel_fromat;
    config.frame_size    = frame_size;
    config.jpeg_quality  = 12;
    config.fb_count      = fb_count;
    config.fb_location   = CAMERA_FB_IN_PSRAM;
    config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
    config.fb_count  = 1;

    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Camera init failed with error 0x%x", err);
        return;
    }

    ESP_LOGI(TAG, "Camera init success!");
    sensor_t *s = esp_camera_sensor_get();
    if (s->id.PID == OV2640_PID || s->id.PID == OV3660_PID)
        s->set_vflip(s, 1);

    xQueueFrameO = frame_o;
    xTaskCreatePinnedToCore(task_process_handler, TAG, 8 * 1024, NULL, 5, NULL, 1);
}