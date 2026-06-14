// ===== BIT-BANG: CS stays LOW during cmd+data =====
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/gpio.h"

#define TAG "BB"

static const int8_t SCL=16, SDA=18, DC=33, CS=35, RST=39;

#define lo(p) gpio_set_level(p, 0)
#define hi(p) gpio_set_level(p, 1)

static void dly(void) {
    for(volatile int i=0; i<30; i++) __asm__ __volatile__("nop");
}

static void wb(uint8_t b) {
    for(int i=7; i>=0; i--) {
        if(b & (1<<i)) hi(SDA); else lo(SDA);
        dly(); hi(SCL); dly(); lo(SCL);
    }
}

// Send command + data bytes with CS held low throughout
static void cmd_dat(uint8_t cmd, const uint8_t *data, int len) {
    lo(CS);
    lo(DC); wb(cmd);  // command
    if(len > 0) {
        hi(DC); for(int i=0; i<len; i++) wb(data[i]); // data
    }
    hi(CS);
}

static void cmd(uint8_t c) { lo(CS); lo(DC); wb(c); hi(CS); }

static void fill(uint16_t color) {
    uint8_t d[4];
    d[0]=0; d[1]=0; d[2]=0; d[3]=239;
    cmd_dat(0x2A, d, 4);
    cmd_dat(0x2B, d, 4);
    
    // RAMWR with 57600 pixels, CS low for the whole thing
    lo(CS); lo(DC); wb(0x2C);
    hi(DC);
    for(uint32_t i=0; i<57600; i++) { wb(color>>8); wb(color&0xFF); }
    hi(CS);
}

static void reset(void) {
    hi(RST); vTaskDelay(100);
    lo(RST); vTaskDelay(100);
    hi(RST); vTaskDelay(200);
}

// ====== DIFFERENT INIT SEQUENCES ======

static void try_gc9a01_full(void) {
    ESP_LOGI(TAG, "GC9A01 FULL init");
    cmd(0x01); vTaskDelay(200);
    cmd(0x11); vTaskDelay(200);
    { uint8_t d[]={0x00}; cmd_dat(0x36,d,1); }
    { uint8_t d[]={0x05}; cmd_dat(0x3A,d,1); }
    { uint8_t d[]={0x00,0x00,0x00,0xEF}; cmd_dat(0x2A,d,4); }
    { uint8_t d[]={0x00,0x00,0x00,0xEF}; cmd_dat(0x2B,d,4); }
    cmd(0x20); vTaskDelay(10); // Inversion OFF
    cmd(0x13); vTaskDelay(10);
    cmd(0x29); vTaskDelay(100);
}

static void try_gc9a01_inv_on(void) {
    ESP_LOGI(TAG, "GC9A01 INV ON");
    cmd(0x01); vTaskDelay(200);
    cmd(0x11); vTaskDelay(200);
    { uint8_t d[]={0x00}; cmd_dat(0x36,d,1); }
    { uint8_t d[]={0x05}; cmd_dat(0x3A,d,1); }
    { uint8_t d[]={0x00,0x00,0x00,0xEF}; cmd_dat(0x2A,d,4); }
    { uint8_t d[]={0x00,0x00,0x00,0xEF}; cmd_dat(0x2B,d,4); }
    cmd(0x21); vTaskDelay(10); // Inversion ON
    cmd(0x13); vTaskDelay(10);
    cmd(0x29); vTaskDelay(100);
}

static void try_st7789_full(void) {
    ESP_LOGI(TAG, "ST7789 FULL init");
    cmd(0x01); vTaskDelay(200);
    cmd(0x11); vTaskDelay(200);
    { uint8_t d[]={0x00}; cmd_dat(0x36,d,1); }
    { uint8_t d[]={0x05}; cmd_dat(0x3A,d,1); }
    { uint8_t d[]={0x0C,0x0C,0x00,0x33,0x33}; cmd_dat(0xB2,d,5); }
    { uint8_t d[]={0x35}; cmd_dat(0xB7,d,1); }
    { uint8_t d[]={0x19}; cmd_dat(0xBB,d,1); }
    { uint8_t d[]={0x2C}; cmd_dat(0xC0,d,1); }
    { uint8_t d[]={0x01}; cmd_dat(0xC2,d,1); }
    { uint8_t d[]={0x12}; cmd_dat(0xC3,d,1); }
    { uint8_t d[]={0x20}; cmd_dat(0xC4,d,1); }
    { uint8_t d[]={0x0F}; cmd_dat(0xC6,d,1); }
    { uint8_t d[]={0xA4,0xA1}; cmd_dat(0xD0,d,2); }
    { uint8_t d[]={0xD0,0x04,0x0D,0x11,0x13,0x2B,0x3F,0x54,0x4C,0x18,0x0D,0x0B,0x1F,0x23}; cmd_dat(0xE0,d,14); }
    { uint8_t d[]={0xD0,0x04,0x0C,0x11,0x13,0x2C,0x3F,0x44,0x51,0x2F,0x1F,0x1F,0x20,0x23}; cmd_dat(0xE1,d,14); }
    cmd(0x21); vTaskDelay(10);
    cmd(0x13); vTaskDelay(10);
    cmd(0x29); vTaskDelay(100);
}

static void try_no_init(void) {
    ESP_LOGI(TAG, "NO INIT - just fill after reset");
    cmd(0x01); vTaskDelay(200);
    cmd(0x11); vTaskDelay(200);
    cmd(0x29); vTaskDelay(50);
}

typedef struct {
    const char *name;
    void (*fn)(void);
    uint16_t color;
} test_t;

void app_main(void) {
    ESP_LOGI(TAG, "=== CS-LOW SCANNER ===");
    
    gpio_reset_pin(SCL); gpio_set_direction(SCL, GPIO_MODE_OUTPUT);
    gpio_reset_pin(SDA); gpio_set_direction(SDA, GPIO_MODE_OUTPUT);
    gpio_reset_pin(DC);  gpio_set_direction(DC, GPIO_MODE_OUTPUT);
    gpio_reset_pin(CS);  gpio_set_direction(CS, GPIO_MODE_OUTPUT);
    gpio_reset_pin(RST); gpio_set_direction(RST, GPIO_MODE_OUTPUT);
    lo(SCL); lo(SDA); hi(CS); lo(DC);

    test_t tests[] = {
        {"GC9A01 INV OFF", try_gc9a01_full, 0xF800},
        {"GC9A01 INV ON",  try_gc9a01_inv_on, 0x07E0},
        {"ST7789",         try_st7789_full,  0x001F},
        {"NO INIT",        try_no_init,      0xFFFF},
    };
    int n = sizeof(tests)/sizeof(tests[0]);

    for(int i=0; i<n; i++) {
        ESP_LOGI(TAG, "=== Test %d: %s ===", i, tests[i].name);
        reset();
        tests[i].fn();
        ESP_LOGI(TAG, "  Fill 0x%04X 3 sec...", tests[i].color);
        fill(tests[i].color);
        vTaskDelay(3000);
        fill(0x0000);
        vTaskDelay(500);
    }

    ESP_LOGI(TAG, "=== DONE ===");
    while(1) vTaskDelay(1000);
}
