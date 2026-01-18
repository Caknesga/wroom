#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2s.h"
#include "driver/gpio.h"
#include "esp_log.h"

// ================= CONFIG =================
#define TAG "RAM_REC"

// I2S pins (your wiring)

#define I2S_BCLK   GPIO_NUM_36
#define I2S_WS     GPIO_NUM_39
#define I2S_SD     GPIO_NUM_17

// LED
#define LED_GPIO   GPIO_NUM_10

// Audio
#define SAMPLE_RATE     16000
#define RECORD_SECONDS  2
#define NUM_SAMPLES     (SAMPLE_RATE * RECORD_SECONDS)

// I2S
#define I2S_PORT I2S_NUM_0
// ==========================================

static int16_t audio_buffer[NUM_SAMPLES];

// ---------- LED ----------
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

// ---------- I2S ----------
static void i2s_init(void)
{
    i2s_config_t i2s_config = {
        .mode = I2S_MODE_MASTER | I2S_MODE_RX,
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_I2S,
        .dma_buf_count = 4,
        .dma_buf_len = 256,
        .use_apll = false,
        .tx_desc_auto_clear = false,
        .fixed_mclk = 0
    };

    i2s_pin_config_t pin_config = {
        .bck_io_num = I2S_BCLK,
        .ws_io_num = I2S_WS,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num = I2S_SD
    };

    i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
    i2s_set_pin(I2S_PORT, &pin_config);
    i2s_zero_dma_buffer(I2S_PORT);
}

// ---------- MAIN ----------
void app_main(void)
{
    esp_log_level_set("*", ESP_LOG_NONE); // keep serial clean

    led_init();
    i2s_init();

    // -------- RECORD --------
    gpio_set_level(LED_GPIO, 1);   // LED ON (recording)

    size_t bytes_read;
    int32_t sample32;

    for (int i = 0; i < NUM_SAMPLES; i++) {
        i2s_read(I2S_PORT,
                 &sample32,
                 sizeof(sample32),
                 &bytes_read,
                 portMAX_DELAY);

        // INMP441: 24-bit left-aligned → int16
        audio_buffer[i] = (int16_t)(sample32 >> 16);
    }

    gpio_set_level(LED_GPIO, 0);   // LED OFF (recording done)

    // -------- DUMP --------
    // CSV-like (one sample per line)
    for (int i = 0; i < NUM_SAMPLES; i++) {
        printf("%d\n", audio_buffer[i]);
    }

    // Idle
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}