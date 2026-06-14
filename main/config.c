#include "config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/uart.h"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "CONFIG";

/* ========================================================================== */
/*   ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ — ОПРЕДЕЛЕНИЯ                                     */
/* ========================================================================== */
volatile float setpoint_kPa = 0.0f;
volatile float pressure1_kPa = 0.0f;
uint32_t sum_err = 0;

/* ========================================================================== */
/*   ОБЪЕКТЫ ДИСПЛЕЯ И ШРИФТОВ                                               */
/* ========================================================================== */
TFT_t dev;
FontxFile fx16[2];

/* ========================================================================== */
/*   ЗАДАЧА ПРИЁМА КОМАНД ПО USB UART (UART_NUM_0)                           */
/* ========================================================================== */
void usb_uart_rx_task(void *pvParameters) {
    uint8_t rx_buffer[256];

    while (1) {
        int len = uart_read_bytes(UART_NUM_0, rx_buffer, sizeof(rx_buffer) - 1, pdMS_TO_TICKS(100));
        if (len > 0) {
            rx_buffer[len] = '\0';

            // Парсим команду вида "SET=150.0" — установка уставки
            if (strncmp((char*)rx_buffer, "SET=", 4) == 0) {
                float new_setpoint = atof((char*)(rx_buffer + 4));
                if (new_setpoint >= 0.0f && new_setpoint <= 500.0f) {
                    setpoint_kPa = new_setpoint;
                    ESP_LOGI(TAG, "Уставка установлена: %.1f kPa", (double)setpoint_kPa);
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}
