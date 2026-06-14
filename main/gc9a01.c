#include <string.h>
#include <inttypes.h>
#include <math.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <driver/spi_master.h>
#include <driver/gpio.h>
#include "esp_log.h"

#include "gc9a01.h"
#include "config.h"

#define TAG "GC9A01"
#define	_DEBUG_ 0

#if CONFIG_SPI2_HOST
#define HOST_ID SPI2_HOST
#elif CONFIG_SPI3_HOST
#define HOST_ID SPI3_HOST
#endif

#define SPI_DEFAULT_FREQUENCY SPI_MASTER_FREQ_20M; // 20MHz

static const int SPI_Command_Mode = 0;
static const int SPI_Data_Mode = 1;

int clock_speed_hz = SPI_DEFAULT_FREQUENCY;

void spi_clock_speed(int speed) {
	ESP_LOGI(TAG, "SPI clock speed=%d MHz", speed/1000000);
	clock_speed_hz = speed;
}

void spi_master_init(TFT_t * dev, int16_t GPIO_MOSI, int16_t GPIO_SCLK, int16_t GPIO_CS, int16_t GPIO_DC, int16_t GPIO_RESET, int16_t GPIO_BL)
{
	esp_err_t ret;

	ESP_LOGI(TAG, "GPIO_CS=%d",GPIO_CS);
	if ( GPIO_CS >= 0 ) {
		// Hardware CS: just reset pin, SPI driver controls it
		gpio_reset_pin( GPIO_CS );
		gpio_set_direction( GPIO_CS, GPIO_MODE_OUTPUT );
		// Do NOT set CS level — SPI driver handles CS via spics_io_num
	}

	ESP_LOGI(TAG, "GPIO_DC=%d",GPIO_DC);
	gpio_reset_pin( GPIO_DC );
	gpio_set_direction( GPIO_DC, GPIO_MODE_OUTPUT );
	gpio_set_level( GPIO_DC, 0 );

	ESP_LOGI(TAG, "GPIO_RESET=%d",GPIO_RESET);
	if ( GPIO_RESET >= 0 ) {
		gpio_reset_pin( GPIO_RESET );
		gpio_set_direction( GPIO_RESET, GPIO_MODE_OUTPUT );
		gpio_set_level( GPIO_RESET, 1 );
		delayMS(100);
		gpio_set_level( GPIO_RESET, 0 );
		delayMS(100);
		gpio_set_level( GPIO_RESET, 1 );
		delayMS(100);
	}

	ESP_LOGI(TAG, "GPIO_BL=%d",GPIO_BL);
	if ( GPIO_BL >= 0 ) {
		gpio_reset_pin(GPIO_BL);
		gpio_set_direction( GPIO_BL, GPIO_MODE_OUTPUT );
		gpio_set_level( GPIO_BL, 0 );
	}

	ESP_LOGI(TAG, "GPIO_MOSI=%d",GPIO_MOSI);
	ESP_LOGI(TAG, "GPIO_SCLK=%d",GPIO_SCLK);
	spi_bus_config_t buscfg = {
		.mosi_io_num = GPIO_MOSI,
		.miso_io_num = -1,
		.sclk_io_num = GPIO_SCLK,
		.quadwp_io_num = -1,
		.quadhd_io_num = -1,
		.max_transfer_sz = 0,
		.flags = 0
	};

	ret = spi_bus_initialize( HOST_ID, &buscfg, SPI_DMA_CH_AUTO );
	ESP_LOGD(TAG, "spi_bus_initialize=%d",ret);
	assert(ret==ESP_OK);

	spi_device_interface_config_t devcfg;
	memset(&devcfg, 0, sizeof(devcfg));
	devcfg.clock_speed_hz = clock_speed_hz;
	devcfg.queue_size = 7;
	devcfg.mode = 3;
	devcfg.flags = SPI_DEVICE_NO_DUMMY;

	if ( GPIO_CS >= 0 ) {
		devcfg.spics_io_num = GPIO_CS;
	} else {
		devcfg.spics_io_num = -1;
	}
	
	spi_device_handle_t handle;
	ret = spi_bus_add_device( HOST_ID, &devcfg, &handle);
	ESP_LOGD(TAG, "spi_bus_add_device=%d",ret);
	assert(ret==ESP_OK);
	dev->_dc = GPIO_DC;
	dev->_bl = GPIO_BL;
	dev->_SPIHandle = handle;
}

bool spi_master_write_byte(spi_device_handle_t SPIHandle, const uint8_t* Data, size_t DataLength)
{
	spi_transaction_t SPITransaction;
	esp_err_t ret;

	if ( DataLength > 0 ) {
		memset( &SPITransaction, 0, sizeof( spi_transaction_t ) );
		SPITransaction.length = DataLength * 8;
		SPITransaction.tx_buffer = Data;
		ret = spi_device_transmit( SPIHandle, &SPITransaction );
		assert(ret==ESP_OK); 
	}

	return true;
}

bool spi_master_write_command(TFT_t * dev, uint8_t cmd)
{
	static uint8_t Byte = 0;
	Byte = cmd;
	gpio_set_level( dev->_dc, SPI_Command_Mode );
	return spi_master_write_byte( dev->_SPIHandle, &Byte, 1 );
}

bool spi_master_write_data_byte(TFT_t * dev, uint8_t data)
{
	static uint8_t Byte = 0;
	Byte = data;
	gpio_set_level( dev->_dc, SPI_Data_Mode );
	return spi_master_write_byte( dev->_SPIHandle, &Byte, 1 );
}


bool spi_master_write_data_word(TFT_t * dev, uint16_t data)
{
	static uint8_t Byte[2];
	Byte[0] = (data >> 8) & 0xFF;
	Byte[1] = data & 0xFF;
	gpio_set_level( dev->_dc, SPI_Data_Mode );
	return spi_master_write_byte( dev->_SPIHandle, Byte, 2);
}

bool spi_master_write_addr(TFT_t * dev, uint16_t addr1, uint16_t addr2)
{
	static uint8_t Byte[4];
	Byte[0] = (addr1 >> 8) & 0xFF;
	Byte[1] = addr1 & 0xFF;
	Byte[2] = (addr2 >> 8) & 0xFF;
	Byte[3] = addr2 & 0xFF;
	gpio_set_level( dev->_dc, SPI_Data_Mode );
	return spi_master_write_byte( dev->_SPIHandle, Byte, 4);
}

bool spi_master_write_color(TFT_t * dev, uint16_t color, uint16_t size)
{
	static uint8_t Byte[1024];
	int index = 0;
	for(int i=0;i<size;i++) {
		Byte[index++] = (color >> 8) & 0xFF;
		Byte[index++] = color & 0xFF;
	}
	gpio_set_level( dev->_dc, SPI_Data_Mode );
	return spi_master_write_byte( dev->_SPIHandle, Byte, size*2);
}

bool spi_master_write_colors(TFT_t * dev, uint16_t * colors, uint16_t size)
{
	static uint8_t Byte[1024];
	int index = 0;
	for(int i=0;i<size;i++) {
		Byte[index++] = (colors[i] >> 8) & 0xFF;
		Byte[index++] = colors[i] & 0xFF;
	}
	gpio_set_level( dev->_dc, SPI_Data_Mode );
	return spi_master_write_byte( dev->_SPIHandle, Byte, size*2);
}

void delayMS(int ms) {
	int _ms = ms + (portTICK_PERIOD_MS - 1);
	TickType_t xTicksToDelay = _ms / portTICK_PERIOD_MS;
	vTaskDelay(xTicksToDelay);
}


// ==========================================================================
// GC9A01 Initialization (240x240 round display)
// Based on GC9A01 datasheet initialization sequence
// ==========================================================================
void lcdInit(TFT_t * dev, int width, int height, int offsetx, int offsety)
{
	dev->_width = width;
	dev->_height = height;
	dev->_offsetx = offsetx;
	dev->_offsety = offsety;
	dev->_font_direction = DIRECTION0;
	dev->_font_fill = false;
	dev->_font_underline = false;

	// --- GC9A01 Initialization Sequence ---

	// Inter register enable 1
	spi_master_write_command(dev, 0xFE);
	spi_master_write_command(dev, 0xEF);
	
	// Inter register enable 2
	spi_master_write_command(dev, 0xEB);
	spi_master_write_data_byte(dev, 0x14);

	// Power control
	spi_master_write_command(dev, 0x84);
	spi_master_write_data_byte(dev, 0x40);
	spi_master_write_command(dev, 0x85);
	spi_master_write_data_byte(dev, 0xFF);
	spi_master_write_command(dev, 0x86);
	spi_master_write_data_byte(dev, 0xFF);
	spi_master_write_command(dev, 0x87);
	spi_master_write_data_byte(dev, 0xFF);
	spi_master_write_command(dev, 0x88);
	spi_master_write_data_byte(dev, 0x0A);
	spi_master_write_command(dev, 0x89);
	spi_master_write_data_byte(dev, 0x21);
	spi_master_write_command(dev, 0x8A);
	spi_master_write_data_byte(dev, 0x00);
	spi_master_write_command(dev, 0x8B);
	spi_master_write_data_byte(dev, 0x80);
	spi_master_write_command(dev, 0x8C);
	spi_master_write_data_byte(dev, 0x01);
	spi_master_write_command(dev, 0x8D);
	spi_master_write_data_byte(dev, 0x01);
	spi_master_write_command(dev, 0x8E);
	spi_master_write_data_byte(dev, 0xFF);
	spi_master_write_command(dev, 0x8F);
	spi_master_write_data_byte(dev, 0xFF);

	// Display Function Control
	spi_master_write_command(dev, 0xB6);
	spi_master_write_data_byte(dev, 0x00);
	spi_master_write_data_byte(dev, 0x20);

	// MADCTL: Memory Data Access Control
	// Try different MADCTL if colors/rotation wrong: 0x00(RGB), 0x08(BGR), 0x48(mirror BGR), 0x88(rotate BGR)
	spi_master_write_command(dev, 0x36);
	spi_master_write_data_byte(dev, 0x00);

	// COLMOD: Interface Pixel Format — 16-bit/pixel (65K colors)
	spi_master_write_command(dev, 0x3A);
	spi_master_write_data_byte(dev, 0x05);

	// CASET: Column Address Set (0..239)
	spi_master_write_command(dev, 0x2A);
	spi_master_write_data_byte(dev, 0x00);
	spi_master_write_data_byte(dev, 0x00);
	spi_master_write_data_byte(dev, 0x00);
	spi_master_write_data_byte(dev, 0xEF);

	// RASET: Row Address Set (0..239)
	spi_master_write_command(dev, 0x2B);
	spi_master_write_data_byte(dev, 0x00);
	spi_master_write_data_byte(dev, 0x00);
	spi_master_write_data_byte(dev, 0x00);
	spi_master_write_data_byte(dev, 0xEF);

	// VCOM setting
	spi_master_write_command(dev, 0x90);
	spi_master_write_data_byte(dev, 0x08);
	spi_master_write_data_byte(dev, 0x08);
	spi_master_write_data_byte(dev, 0x08);
	spi_master_write_data_byte(dev, 0x08);

	spi_master_write_command(dev, 0xBD);
	spi_master_write_data_byte(dev, 0x06);
	spi_master_write_command(dev, 0xBC);
	spi_master_write_data_byte(dev, 0x00);

	spi_master_write_command(dev, 0xFF);
	spi_master_write_data_byte(dev, 0x60);
	spi_master_write_data_byte(dev, 0x01);
	spi_master_write_data_byte(dev, 0x04);

	// Power Control 2
	spi_master_write_command(dev, 0xC3);
	spi_master_write_data_byte(dev, 0x13);
	spi_master_write_command(dev, 0xC4);
	spi_master_write_data_byte(dev, 0x13);

	spi_master_write_command(dev, 0xC9);
	spi_master_write_data_byte(dev, 0x22);

	spi_master_write_command(dev, 0xBE);
	spi_master_write_data_byte(dev, 0x11);

	spi_master_write_command(dev, 0xE1);
	spi_master_write_data_byte(dev, 0x10);
	spi_master_write_data_byte(dev, 0x0E);

	spi_master_write_command(dev, 0xDF);
	spi_master_write_data_byte(dev, 0x21);
	spi_master_write_data_byte(dev, 0x0C);
	spi_master_write_data_byte(dev, 0x02);

	// Gamma Set
	{
		uint8_t gamma[6];
		spi_master_write_command(dev, 0xF0);
		gamma[0]=0x45; gamma[1]=0x09; gamma[2]=0x08; gamma[3]=0x08; gamma[4]=0x26; gamma[5]=0x2A;
		for(int i=0;i<6;i++) spi_master_write_data_byte(dev, gamma[i]);

		spi_master_write_command(dev, 0xF1);
		gamma[0]=0x43; gamma[1]=0x70; gamma[2]=0x72; gamma[3]=0x36; gamma[4]=0x37; gamma[5]=0x6F;
		for(int i=0;i<6;i++) spi_master_write_data_byte(dev, gamma[i]);

		spi_master_write_command(dev, 0xF2);
		gamma[0]=0x45; gamma[1]=0x09; gamma[2]=0x08; gamma[3]=0x08; gamma[4]=0x26; gamma[5]=0x2A;
		for(int i=0;i<6;i++) spi_master_write_data_byte(dev, gamma[i]);

		spi_master_write_command(dev, 0xF3);
		gamma[0]=0x43; gamma[1]=0x70; gamma[2]=0x72; gamma[3]=0x36; gamma[4]=0x37; gamma[5]=0x6F;
		for(int i=0;i<6;i++) spi_master_write_data_byte(dev, gamma[i]);
	}

	spi_master_write_command(dev, 0xED);
	spi_master_write_data_byte(dev, 0x1B);
	spi_master_write_data_byte(dev, 0x0B);

	spi_master_write_command(dev, 0xAE);
	spi_master_write_data_byte(dev, 0x77);

	spi_master_write_command(dev, 0xCD);
	spi_master_write_data_byte(dev, 0x63);

	// Frame Rate Control
	{
		uint8_t frate[] = {0x07, 0x07, 0x04, 0x0E, 0x0F, 0x09, 0x07, 0x08, 0x03};
		spi_master_write_command(dev, 0x70);
		for(int i=0;i<9;i++) spi_master_write_data_byte(dev, frate[i]);
	}

	spi_master_write_command(dev, 0xE8);
	spi_master_write_data_byte(dev, 0x34);

	// AVDD / AVCL / VGH / VGL Settings
	{
		uint8_t pwr[12];
		spi_master_write_command(dev, 0x62);
		pwr[0]=0x18; pwr[1]=0x0D; pwr[2]=0x71; pwr[3]=0xED; pwr[4]=0x70; pwr[5]=0x70;
		pwr[6]=0x18; pwr[7]=0x0F; pwr[8]=0x71; pwr[9]=0xEF; pwr[10]=0x70; pwr[11]=0x70;
		for(int i=0;i<12;i++) spi_master_write_data_byte(dev, pwr[i]);

		spi_master_write_command(dev, 0x63);
		pwr[0]=0x18; pwr[1]=0x11; pwr[2]=0x71; pwr[3]=0xF1; pwr[4]=0x70; pwr[5]=0x70;
		pwr[6]=0x18; pwr[7]=0x13; pwr[8]=0x71; pwr[9]=0xF3; pwr[10]=0x70; pwr[11]=0x70;
		for(int i=0;i<12;i++) spi_master_write_data_byte(dev, pwr[i]);

		spi_master_write_command(dev, 0x64);
		uint8_t pwr3[] = {0x28, 0x29, 0xF1, 0x01, 0xF1, 0x00, 0x07};
		for(int i=0;i<7;i++) spi_master_write_data_byte(dev, pwr3[i]);

		spi_master_write_command(dev, 0x66);
		uint8_t pwr4[] = {0x3C, 0x00, 0xCD, 0x67, 0x45, 0x45, 0x10, 0x00, 0x00, 0x00};
		for(int i=0;i<10;i++) spi_master_write_data_byte(dev, pwr4[i]);

		spi_master_write_command(dev, 0x67);
		uint8_t pwr5[] = {0x00, 0x3C, 0x00, 0x00, 0x00, 0x01, 0x54, 0x10, 0x32, 0x98};
		for(int i=0;i<10;i++) spi_master_write_data_byte(dev, pwr5[i]);

		spi_master_write_command(dev, 0x74);
		uint8_t pwr6[] = {0x10, 0x85, 0x80, 0x00, 0x00, 0x4E, 0x00};
		for(int i=0;i<7;i++) spi_master_write_data_byte(dev, pwr6[i]);
	}

	spi_master_write_command(dev, 0x98);
	spi_master_write_data_byte(dev, 0x3E);
	spi_master_write_data_byte(dev, 0x07);

	// Tearing Effect Line ON
	spi_master_write_command(dev, 0x35);
	spi_master_write_data_byte(dev, 0x00);

	// Display Inversion On (many GC9A01 modules need this)
	spi_master_write_command(dev, 0x21);
	delayMS(10);

	// Normal Display Mode
	spi_master_write_command(dev, 0x13);
	delayMS(10);

	// Sleep Out
	spi_master_write_command(dev, 0x11);
	delayMS(150);

	// Display ON
	spi_master_write_command(dev, 0x29);
	delayMS(50);

	// Backlight ON (if BL pin is set)
	if(dev->_bl >= 0) {
		gpio_set_level( dev->_bl, 1 );
	} else {
		// Even if no BL GPIO, the module backlight may be hardwired to VCC
		ESP_LOGI(TAG, "No BL pin configured — backlight should be hardwired to VCC");
	}

	dev->_use_frame_buffer = false;
}


// Draw pixel
// x:X coordinate
// y:Y coordinate
// color:color
void lcdDrawPixel(TFT_t * dev, uint16_t x, uint16_t y, uint16_t color){
	if (x >= dev->_width) return;
	if (y >= dev->_height) return;

	if (dev->_use_frame_buffer) {
		dev->_frame_buffer[y*dev->_width+x] = color;
	} else {
		uint16_t _x = x + dev->_offsetx;
		uint16_t _y = y + dev->_offsety;

		spi_master_write_command(dev, 0x2A);	// set column(x) address
		spi_master_write_addr(dev, _x, _x);
		spi_master_write_command(dev, 0x2B);	// set Page(y) address
		spi_master_write_addr(dev, _y, _y);
		spi_master_write_command(dev, 0x2C);	// Memory Write
		spi_master_write_colors(dev, &color, 1);
	}
}


// Draw multi pixel
void lcdDrawMultiPixels(TFT_t * dev, uint16_t x, uint16_t y, uint16_t size, uint16_t * colors) {
	if (x+size > dev->_width) return;
	if (y >= dev->_height) return;

	if (dev->_use_frame_buffer) {
		uint16_t _x1 = x;
		uint16_t _x2 = _x1 + (size-1);
		uint16_t _y1 = y;
		uint16_t _y2 = _y1;
		int16_t index = 0;
		for (int16_t j = _y1; j <= _y2; j++){
			for(int16_t i = _x1; i <= _x2; i++){
				 dev->_frame_buffer[j*dev->_width+i] = colors[index++];
			}
		}
	} else {
		uint16_t _x1 = x + dev->_offsetx;
		uint16_t _x2 = _x1 + (size-1);
		uint16_t _y1 = y + dev->_offsety;
		uint16_t _y2 = _y1;

		spi_master_write_command(dev, 0x2A);
		spi_master_write_addr(dev, _x1, _x2);
		spi_master_write_command(dev, 0x2B);
		spi_master_write_addr(dev, _y1, _y2);
		spi_master_write_command(dev, 0x2C);
		spi_master_write_colors(dev, colors, size);
	}
}

// Draw rectangle of filling
void lcdDrawFillRect(TFT_t * dev, uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color) {
	if (x1 >= dev->_width) return;
	if (x2 >= dev->_width) x2=dev->_width-1;
	if (y1 >= dev->_height) return;
	if (y2 >= dev->_height) y2=dev->_height-1;

	if (dev->_use_frame_buffer) {
		for (int16_t j = y1; j <= y2; j++){
			for(int16_t i = x1; i <= x2; i++){
				dev->_frame_buffer[j*dev->_width+i] = color;
			}
		}
	} else {
		uint16_t _x1 = x1 + dev->_offsetx;
		uint16_t _x2 = x2 + dev->_offsetx;
		uint16_t _y1 = y1 + dev->_offsety;
		uint16_t _y2 = y2 + dev->_offsety;

		spi_master_write_command(dev, 0x2A);
		spi_master_write_addr(dev, _x1, _x2);
		spi_master_write_command(dev, 0x2B);
		spi_master_write_addr(dev, _y1, _y2);
		spi_master_write_command(dev, 0x2C);
		spi_master_write_color(dev, color, (x2-x1+1)*(y2-y1+1));
	}
}

void lcdDrawFillSquare(TFT_t * dev, uint16_t x0, uint16_t y0, uint16_t size, uint16_t color) {
	lcdDrawFillRect(dev, x0, y0, x0+size-1, y0+size-1, color);
}

void lcdDisplayOff(TFT_t * dev) {
	spi_master_write_command(dev, 0x28);
}
 
void lcdDisplayOn(TFT_t * dev) {
	spi_master_write_command(dev, 0x29);
}

void lcdFillScreen(TFT_t * dev, uint16_t color) {
	lcdDrawFillRect(dev, 0, 0, dev->_width-1, dev->_height-1, color);
}

void lcdDrawLine(TFT_t * dev, uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color) {
	int i;
	int dx,dy;
	int sx,sy;
	int E;

	/* distance between two points */
	dx = ( x2 > x1 ) ? x2 - x1 : x1 - x2;
	dy = ( y2 > y1 ) ? y2 - y1 : y1 - y2;

	/* direction of two point */
	sx = ( x2 > x1 ) ? 1 : -1;
	sy = ( y2 > y1 ) ? 1 : -1;

	/* inclination < 1 */
	if ( dx > dy ) {
		E = -dx;
		for ( i = 0 ; i <= dx ; i++ ) {
			lcdDrawPixel(dev, x1, y1, color);
			x1 += sx;
			E += 2 * dy;
			if ( E >= 0 ) {
			y1 += sy;
			E -= 2 * dx;
		}
	}
	} else {
		E = -dy;
		for ( i = 0 ; i <= dy ; i++ ) {
			lcdDrawPixel(dev, x1, y1, color);
			y1 += sy;
			E += 2 * dx;
			if ( E >= 0 ) {
				x1 += sx;
				E -= 2 * dy;
			}
		}
	}
}

void lcdDrawRect(TFT_t * dev, uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color) {
	lcdDrawLine(dev, x1, y1, x2, y1, color);
	lcdDrawLine(dev, x2, y1, x2, y2, color);
	lcdDrawLine(dev, x2, y2, x1, y2, color);
	lcdDrawLine(dev, x1, y2, x1, y1, color);
}

void lcdDrawRectAngle(TFT_t * dev, uint16_t xc, uint16_t yc, uint16_t w, uint16_t h, uint16_t angle, uint16_t color) {
	double xd,yd,rd;
	int x1,y1;
	int x2,y2;
	int x3,y3;
	int x4,y4;
	rd = -angle * M_PI / 180.0;
	xd = 0.0 - w/2;
	yd = h/2;
	x1 = (int)(xd * cos(rd) - yd * sin(rd) + xc);
	y1 = (int)(xd * sin(rd) + yd * cos(rd) + yc);

	yd = 0.0 - yd;
	x2 = (int)(xd * cos(rd) - yd * sin(rd) + xc);
	y2 = (int)(xd * sin(rd) + yd * cos(rd) + yc);

	xd = w/2;
	yd = h/2;
	x3 = (int)(xd * cos(rd) - yd * sin(rd) + xc);
	y3 = (int)(xd * sin(rd) + yd * cos(rd) + yc);

	yd = 0.0 - yd;
	x4 = (int)(xd * cos(rd) - yd * sin(rd) + xc);
	y4 = (int)(xd * sin(rd) + yd * cos(rd) + yc);

	lcdDrawLine(dev, x1, y1, x2, y2, color);
	lcdDrawLine(dev, x2, y2, x4, y4, color);
	lcdDrawLine(dev, x4, y4, x3, y3, color);
	lcdDrawLine(dev, x3, y3, x1, y1, color);
}

void lcdDrawTriangle(TFT_t * dev, uint16_t xc, uint16_t yc, uint16_t w, uint16_t h, uint16_t angle, uint16_t color) {
	double xd,yd,rd;
	int x1,y1;
	int x2,y2;
	int x3,y3;
	rd = -angle * M_PI / 180.0;
	xd = 0.0;
	yd = h/2;
	x1 = (int)(xd * cos(rd) - yd * sin(rd) + xc);
	y1 = (int)(xd * sin(rd) + yd * cos(rd) + yc);

	xd = w/2;
	yd = 0.0 - yd;
	x2 = (int)(xd * cos(rd) - yd * sin(rd) + xc);
	y2 = (int)(xd * sin(rd) + yd * cos(rd) + yc);

	xd = 0.0 - w/2;
	x3 = (int)(xd * cos(rd) - yd * sin(rd) + xc);
	y3 = (int)(xd * sin(rd) + yd * cos(rd) + yc);

	lcdDrawLine(dev, x1, y1, x2, y2, color);
	lcdDrawLine(dev, x2, y2, x3, y3, color);
	lcdDrawLine(dev, x3, y3, x1, y1, color);
}

void lcdDrawRegularPolygon(TFT_t *dev, uint16_t xc, uint16_t yc, uint16_t n, uint16_t r, uint16_t angle, uint16_t color) {
	double rd,xd,yd;
	int x1,y1;
	int x2,y2;
	int i;

	rd = -angle * M_PI / 180.0;
	for (i = 0; i < n; i++) {
		xd = r * cos(2 * M_PI * i / n);
		yd = r * sin(2 * M_PI * i / n);
		x1 = (int)(xd * cos(rd) - yd * sin(rd) + xc);
		y1 = (int)(xd * sin(rd) + yd * cos(rd) + yc);

		xd = r * cos(2 * M_PI * (i + 1) / n);
		yd = r * sin(2 * M_PI * (i + 1) / n);
		x2 = (int)(xd * cos(rd) - yd * sin(rd) + xc);
		y2 = (int)(xd * sin(rd) + yd * cos(rd) + yc);

		lcdDrawLine(dev, x1, y1, x2, y2, color);
	}
}

void lcdDrawCircle(TFT_t * dev, uint16_t x0, uint16_t y0, uint16_t r, uint16_t color) {
	int x;
	int y;
	int err;
	int old_err;

	x=0;
	y=-r;
	err=2-2*r;
	do{
		lcdDrawPixel(dev, x0-x, y0+y, color); 
		lcdDrawPixel(dev, x0-y, y0-x, color); 
		lcdDrawPixel(dev, x0+x, y0-y, color); 
		lcdDrawPixel(dev, x0+y, y0+x, color); 
		if ((old_err=err)<=x)	err+=++x*2+1;
		if (old_err>y || err>x) err+=++y*2+1;
	} while(y<0);
}

void lcdDrawFillCircle(TFT_t * dev, uint16_t x0, uint16_t y0, uint16_t r, uint16_t color) {
	int x;
	int y;
	int err;
	int old_err;
	int ChangeX;

	x=0;
	y=-r;
	err=2-2*r;
	ChangeX=0;
	do{
		if(ChangeX) {
			lcdDrawLine(dev, x0-x, y0-y, x0-x, y0+y, color);
			lcdDrawLine(dev, x0+x, y0-y, x0+x, y0+y, color);
		}
		ChangeX=(old_err=err)<=x;
		if (ChangeX) err+=++x*2+1;
		if (old_err>y || err>x) err+=++y*2+1;
	} while(y<=0);
}

void lcdDrawRoundRect(TFT_t * dev, uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t r, uint16_t color) {
	int x;
	int y;
	int err;
	int old_err;
	unsigned char temp;

	if(x1>x2) {
		temp=x1; x1=x2; x2=temp;
	}
	if(y1>y2) {
		temp=y1; y1=y2; y2=temp;
	}

	int xi = x1 + r;
	int yi = y1 - r;
	int xj = x2 - r;
	int yj = y2 + r;

	x=0;
	y=-r;
	err=2-2*r;

	do{
		if(x) {
			lcdDrawPixel(dev, xj+x, yj+y, color); 
			lcdDrawPixel(dev, xi-x, yj+y, color); 
			lcdDrawPixel(dev, xj+x, yi-y, color); 
			lcdDrawPixel(dev, xi-x, yi-y, color);
		}
		if ((old_err=err)<=x)	err+=++x*2+1;
		if (old_err>y || err>x) err+=++y*2+1;
	} while(y<0);

	lcdDrawLine(dev, xi, y1, xj, y1, color);
	lcdDrawLine(dev, xi, y2, xj, y2, color);
	lcdDrawLine(dev, x1, yi+r+1, x1, yj-r-1, color);
	lcdDrawLine(dev, x2, yi+r+1, x2, yj-r-1, color);
}

void lcdDrawArrow(TFT_t * dev, uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t w, uint16_t color) {
	double Vx = x1 - x0;
	double Vy = y1 - y0;
	double v = sqrt(Vx*Vx+Vy*Vy);
	double Ux = Vx/v;
	double Uy = Vy/v;

	uint16_t L[2],R[2];
	L[0] = (int)(x1 - Uy*w - Ux*v);
	L[1] = (int)(y1 + Ux*w - Uy*v);
	R[0] = (int)(x1 + Uy*w - Ux*v);
	R[1] = (int)(y1 - Ux*w - Uy*v);

	lcdDrawLine(dev, x0, y0, x1, y1, color);
	lcdDrawLine(dev, x1, y1, L[0], L[1], color);
	lcdDrawLine(dev, x1, y1, R[0], R[1], color);
	lcdDrawLine(dev, L[0], L[1], R[0], R[1], color);
}

void lcdDrawFillArrow(TFT_t * dev, uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t w, uint16_t color) {
	double Vx = x1 - x0;
	double Vy = y1 - y0;
	double v = sqrt(Vx*Vx+Vy*Vy);
	double Ux = Vx/v;
	double Uy = Vy/v;

	uint16_t L[2],R[2];
	L[0] = (int)(x1 - Uy*w - Ux*v);
	L[1] = (int)(y1 + Ux*w - Uy*v);
	R[0] = (int)(x1 + Uy*w - Ux*v);
	R[1] = (int)(y1 - Ux*w - Uy*v);

	lcdDrawLine(dev, x0, y0, x1, y1, color);
	lcdDrawLine(dev, x1, y1, L[0], L[1], color);
	lcdDrawLine(dev, x1, y1, R[0], R[1], color);
	lcdDrawLine(dev, L[0], L[1], R[0], R[1], color);

	int ww;
	for(ww=w-1; ww>0; ww--) {
		L[0] = (int)(x1 - Uy*ww - Ux*v);
		L[1] = (int)(y1 + Ux*ww - Uy*v);
		R[0] = (int)(x1 + Uy*ww - Ux*v);
		R[1] = (int)(y1 - Ux*ww - Uy*v);
		lcdDrawLine(dev, x1, y1, L[0], L[1], color);
		lcdDrawLine(dev, x1, y1, R[0], R[1], color);
	}
}

int lcdDrawChar(TFT_t * dev, FontxFile *fxs, uint16_t x, uint16_t y, uint8_t ascii, uint16_t color) {
	uint16_t xx,yy,bit,ofs;
	unsigned char pw, ph;
	int h,w;
	uint16_t mask;
	bool rc;

	rc = GetFontx(fxs, ascii, &pw, &ph);
	if (!rc) return 0;

	int16_t xd1 = 0;
	int16_t yd1 = 0;
	int16_t xd2 = 0;
	int16_t yd2 = 0;
	uint16_t xss = 0;
	uint16_t yss = 0;
	int16_t xsd = 0;
	int16_t ysd = 0;
	int16_t next = 0;
	uint16_t x0 = 0;
	uint16_t x1 = 0;
	uint16_t y0 = 0;
	uint16_t y1 = 0;
	if (dev->_font_direction == DIRECTION0) {
		xd1 = +1;
		yd1 = +1;
		xd2 =  0;
		yd2 =  0;
		xss =  x;
		yss =  y - (ph - 1);
		xsd =  1;
		ysd =  0;
		next = x + pw;
		x0 = x;
		x1 = x + (pw - 1);
		y0 = y - (ph-1);
		y1 = y;
	} else if (dev->_font_direction == DIRECTION90) {
		xd1 =  0;
		yd1 =  0;
		xd2 = -1;
		yd2 = +1;
		xss =  x + ph;
		yss =  y;
		xsd =  0;
		ysd =  1;
		next = y + pw;
		x0 = x;
		x1 = x + (ph - 1);
		y0 = y;
		y1 = y + (pw - 1);
	} else if (dev->_font_direction == DIRECTION180) {
		xd1 = -1;
		yd1 = -1;
		xd2 =  0;
		yd2 =  0;
		xss =  x;
		yss =  y + ph + 1;
		xsd =  1;
		ysd =  0;
		next = x - pw;
		x0 = x - (pw - 1);
		x1 = x;
		y0 = y;
		y1 = y + (ph - 1);
	} else if (dev->_font_direction == DIRECTION270) {
		xd1 =  0;
		yd1 =  0;
		xd2 = +1;
		yd2 = -1;
		xss =  x - (ph - 1);
		yss =  y;
		xsd =  0;
		ysd =  1;
		next = y - pw;
		x0 = x - (ph - 1);
		x1 = x;
		y0 = y - (pw - 1);
		y1 = y;
	}

	if (dev->_font_fill) lcdDrawFillRect(dev, x0, y0, x1, y1, dev->_font_fill_color);

	int bits;
	ofs = 0;
	yy = yss;
	xx = xss;
	for(h=0;h<ph;h++) {
		if(xsd) xx = xss;
		if(ysd) yy = yss;
		bits = pw;
		for(w=0;w<((pw+4)/8);w++) {
			mask = 0x80;
			for(bit=0;bit<8;bit++) {
				bits--;
				if (bits < 0) continue;
				if (fxs->fonts[ofs] & mask) {
					lcdDrawPixel(dev, xx, yy, color);
				}
				if (h == (ph-2) && dev->_font_underline)
					lcdDrawPixel(dev, xx, yy, dev->_font_underline_color);
				if (h == (ph-1) && dev->_font_underline)
					lcdDrawPixel(dev, xx, yy, dev->_font_underline_color);
				xx = xx + xd1;
				yy = yy + yd2;
				mask = mask >> 1;
			}
			ofs++;
		}
		yy = yy + yd1;
		xx = xx + xd2;
	}

	if (dev->_font_direction == DIRECTION0) return next+1;
	if (dev->_font_direction == DIRECTION90) return next;
	if (dev->_font_direction == DIRECTION180) return next-1;
	if (dev->_font_direction == DIRECTION270) return next;
	return 0;
}

int lcdDrawString(TFT_t * dev, FontxFile *fx, uint16_t x, uint16_t y, uint8_t * ascii, uint16_t color) {
	int length = strlen((char *)ascii);
	for(int i=0;i<length;i++) {
		if (dev->_font_direction == 0)
			x = lcdDrawChar(dev, fx, x, y, ascii[i], color);
		if (dev->_font_direction == 1)
			y = lcdDrawChar(dev, fx, x, y, ascii[i], color);
		if (dev->_font_direction == 2)
			x = lcdDrawChar(dev, fx, x, y, ascii[i], color);
		if (dev->_font_direction == 3)
			y = lcdDrawChar(dev, fx, x, y, ascii[i], color);
	}
	if (dev->_font_direction == 0) return x;
	if (dev->_font_direction == 1) return y;
	if (dev->_font_direction == 2) return x;
	if (dev->_font_direction == 3) return y;
	return 0;
}

int lcdDrawCode(TFT_t * dev, FontxFile *fx, uint16_t x,uint16_t y,uint8_t code,uint16_t color) {
	if (dev->_font_direction == 0)
		x = lcdDrawChar(dev, fx, x, y, code, color);
	if (dev->_font_direction == 1)
		y = lcdDrawChar(dev, fx, x, y, code, color);
	if (dev->_font_direction == 2)
		x = lcdDrawChar(dev, fx, x, y, code, color);
	if (dev->_font_direction == 3)
		y = lcdDrawChar(dev, fx, x, y, code, color);
	if (dev->_font_direction == 0) return x;
	if (dev->_font_direction == 1) return y;
	if (dev->_font_direction == 2) return x;
	if (dev->_font_direction == 3) return y;
	return 0;
}

void lcdSetFontDirection(TFT_t * dev, uint16_t dir) {
	dev->_font_direction = dir;
}

void lcdSetFontFill(TFT_t * dev, uint16_t color) {
	dev->_font_fill = true;
	dev->_font_fill_color = color;
}

void lcdUnsetFontFill(TFT_t * dev) {
	dev->_font_fill = false;
}

void lcdSetFontUnderLine(TFT_t * dev, uint16_t color) {
	dev->_font_underline = true;
	dev->_font_underline_color = color;
}

void lcdUnsetFontUnderLine(TFT_t * dev) {
	dev->_font_underline = false;
}

void lcdBacklightOff(TFT_t * dev) {
	if(dev->_bl >= 0) {
		gpio_set_level( dev->_bl, 0 );
	}
}

void lcdBacklightOn(TFT_t * dev) {
	if(dev->_bl >= 0) {
		gpio_set_level( dev->_bl, 1 );
	}
}

void lcdInversionOff(TFT_t * dev) {
	spi_master_write_command(dev, 0x20);
}
 
void lcdInversionOn(TFT_t * dev) {
	spi_master_write_command(dev, 0x21);
}

void lcdWrapArround(TFT_t * dev, SCROLL_TYPE_t scroll, int start, int end) {
	if (dev->_use_frame_buffer) return;

	if (scroll == SCROLL_RIGHT) {
		spi_master_write_command(dev, 0x36);
		spi_master_write_data_byte(dev, 0x60);
	} else if (scroll == SCROLL_LEFT) {
		spi_master_write_command(dev, 0x36);
		spi_master_write_data_byte(dev, 0xA0);
	} else if (scroll == SCROLL_DOWN) {
		spi_master_write_command(dev, 0x36);
		spi_master_write_data_byte(dev, 0xC0);
	} else if (scroll == SCROLL_UP) {
		spi_master_write_command(dev, 0x36);
		spi_master_write_data_byte(dev, 0x00);
	}

	uint16_t _start = start + dev->_offsety;
	uint16_t _end = end + dev->_offsety;
	spi_master_write_command(dev, 0x33);
	spi_master_write_addr(dev, _start, _end);
}

void lcdInversionArea(TFT_t * dev, uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t *save) {
	if (dev->_use_frame_buffer) return;

	uint16_t _x1 = x1 + dev->_offsetx;
	uint16_t _y1 = y1 + dev->_offsety;
	uint16_t _x2 = x2 + dev->_offsetx;
	uint16_t _y2 = y2 + dev->_offsety;

	spi_master_write_command(dev, 0x2A);
	spi_master_write_addr(dev, _x1, _x2);
	spi_master_write_command(dev, 0x2B);
	spi_master_write_addr(dev, _y1, _y2);
	spi_master_write_command(dev, 0x2E); // Read Memory (does not work on most displays)
}
 
void lcdGetRect(TFT_t * dev, uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t *save) {
	lcdInversionArea(dev, x1, y1, x2, y2, save);
}

void lcdSetRect(TFT_t * dev, uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t *save) {
	if (dev->_use_frame_buffer) return;

	uint16_t _x1 = x1 + dev->_offsetx;
	uint16_t _y1 = y1 + dev->_offsety;
	uint16_t _x2 = x2 + dev->_offsetx;
	uint16_t _y2 = y2 + dev->_offsety;
	uint16_t w = x2 - x1 + 1;
	uint16_t h = y2 - y1 + 1;

	spi_master_write_command(dev, 0x2A);
	spi_master_write_addr(dev, _x1, _x2);
	spi_master_write_command(dev, 0x2B);
	spi_master_write_addr(dev, _y1, _y2);
	spi_master_write_command(dev, 0x2C); // Memory Write
	spi_master_write_colors(dev, save, w*h);
}

void lcdSetCursor(TFT_t * dev, uint16_t x0, uint16_t y0, uint16_t r, uint16_t color, uint16_t *save) {
	if (dev->_use_frame_buffer) return;

	// Draw cursor circle
	lcdDrawCircle(dev, x0, y0, r, color);
	lcdDrawCircle(dev, x0, y0, r-1, color);
}

void lcdResetCursor(TFT_t * dev, uint16_t x0, uint16_t y0, uint16_t r, uint16_t color, uint16_t *save) {
	lcdDrawCircle(dev, x0, y0, r, color);
	lcdDrawCircle(dev, x0, y0, r-1, color);
}

void lcdDrawFinish(TFT_t *dev)
{
	// Nothing to do for non-frame-buffer mode
}
