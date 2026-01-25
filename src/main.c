

/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 * Updated for INMP441 I2S Microphone
 */

#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "usb_device_uac.h"
#include "driver/i2s_std.h" // Changed from PDM to STD for INMP441
#include <math.h>

// INMP441 Pin Definitions
#define MIC_I2S_SCK  36  // Clock
#define MIC_I2S_WS   39  // Word Select (LRCLK)
#define MIC_I2S_SD   17  // Serial Data
#define LED_GPIO       GPIO_NUM_10



static i2s_chan_handle_t rx_handle = NULL;

// Callback for USB to pull data from the microphone
static esp_err_t usb_uac_device_input_cb(uint8_t *buf, size_t len, size_t *bytes_read, void *arg)
{
    if (!rx_handle) return ESP_FAIL;

    int16_t *out16 = (int16_t *)buf;
    const size_t samples16 = len / 2;

    static int32_t tmp32[256];
    size_t produced = 0;

    while (produced < samples16) {
        size_t chunk = samples16 - produced;
        if (chunk > 256) chunk = 256;

        size_t br = 0;
        ESP_ERROR_CHECK(i2s_channel_read(rx_handle, tmp32, chunk * 4, &br, portMAX_DELAY));
        size_t got = br / 4;

        for (size_t i = 0; i < got; i++) {
            // Keep the top bits (INMP441 valid data typically sits high in the 32-bit word)
            int32_t s = tmp32[i] >> 16;   // 32 → 16
            s = s * 4;                   // software gain
            if (s > 32767) s = 32767;
            if (s < -32768) s = -32768;
            out16[produced + i] = (int16_t)s;
                    }
        produced += got;
    }

    *bytes_read = len;
    return ESP_OK;
}

// Dummy callbacks for volume/mute since we have no speaker output
static void usb_uac_device_set_mute_cb(uint32_t mute, void *arg) {}
static void usb_uac_device_set_volume_cb(uint32_t volume, void *arg) {}

static void usb_uac_device_init(void)
{
    uac_device_config_t config = {
        .output_cb = NULL, // No speaker output
        .input_cb = usb_uac_device_input_cb,
        .set_mute_cb = usb_uac_device_set_mute_cb,
        .set_volume_cb = usb_uac_device_set_volume_cb,
        .cb_ctx = NULL,
    };
    ESP_ERROR_CHECK(uac_device_init(&config));
}

void init_mic_i2s(void) {
    // 1. Setup Channel
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, NULL, &rx_handle));

    // 2. Setup Standard I2S Configuration (INMP441 uses Philips standard)
    i2s_std_config_t std_cfg = {
        .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(CONFIG_UAC_SAMPLE_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_MONO),

        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = MIC_I2S_SCK,
            .ws   = MIC_I2S_WS,
            .dout = I2S_GPIO_UNUSED,
            .din  = MIC_I2S_SD,
        },
    };
    // INMP441 usually outputs on the Left channel if L/R is tied to GND
    std_cfg.slot_cfg.slot_mask = I2S_STD_SLOT_LEFT; 

    ESP_ERROR_CHECK(i2s_channel_init_std_mode(rx_handle, &std_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(rx_handle));
}

static void led_blink_times(int times, int on_ms, int off_ms)
{
    for (int i = 0; i < times; i++) {
        gpio_set_level(LED_GPIO, 1);
        vTaskDelay(pdMS_TO_TICKS(on_ms));
        gpio_set_level(LED_GPIO, 0);
        vTaskDelay(pdMS_TO_TICKS(off_ms));
    }
}
static void led_init(void)
{
    gpio_config_t cfg = {
        .pin_bit_mask = (1ULL << LED_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = 0,
        .pull_down_en = 0,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&cfg);
    gpio_set_level(LED_GPIO, 0);
}

//function to listen python serial commands
static void serial_cmd_task(void *arg)
{
    while (1) {
        int c = getchar();   // blocks until a byte arrives
        if (c == 'B') {
            gpio_set_level(LED_GPIO, 1);
        } else if (c == 'N') {
            gpio_set_level(LED_GPIO, 0);
        }
    }
}


void app_main(void)
{
    // Initialize Microphone
    init_mic_i2s();
    led_init();
    led_blink_times(5, 200, 200); // Indicate initialization done
    // Initialize USB Audio Class Device
    usb_uac_device_init();
    //xTaskCreate(serial_cmd_task, "serial_cmd", 2048, NULL, 5, NULL);


    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}