// 1. Стандартные системные библиотеки
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "esp_spiffs.h"
#include <string.h>
#include <stdlib.h>
#include "driver/uart.h"

// 2. Ваши собственные модули (интерфейсы управления)
#include "gc9a01.h"           // Для работы с дисплеем GC9A01
#include "pressure_sensor.h"  // Для работы с датчиком давления и калибровкой
#include "config.h"           // Глобальные переменные и общие объекты


#define USB_UART_PORT       UART_NUM_0
#define USB_BUF_SIZE        256



void app_main(void) {
    LCD_init(); 
    pressure_sensor_init(); 
    
    //char screen_buffer[32];
    pressure_ui_and_usb_init(&dev);

    while(1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

}
