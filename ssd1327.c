#include <stdbool.h>							// C standard unit for bool, true, false
#include <stdint.h>								// C standard unit for uint32_t etc
#include <string.h>
#include "spi.h"								// SPI device unit as we will be using SPI
#include "gpio.h"								// We need access to GPIO to resetup reset pin
#include "font8x16.h"							// Font 16x8 bitmap data
#include "font8x8.h"							// Font 8x8 bitmap data
#include "font6x8.h"							// Font 6x8 bitmap data
#include "ssd1327.h"							// This units header

static const uint8_t ssd1327_init[34] = 
{
	0xae,				// Turn screen off while we do changes
	0x15, 0x00, 0x7f,	// set column address start=0 end=127
	0x75, 0x00, 0x7f,	// set row address start=0 end=127
	0x81, 0x80,         // set contrast control to 50%
	0xa0, 0x51,         // gment remap  ... set orientation to normal
	0xa1, 0x00,         // start line 0
	0xa2, 0x00,         // display offset 0
	0xa4,               // rmal display
	0xa8, 0x7f,         // set multiplex ratio
	0xb1, 0xf1,         // set phase length
	0xb3, 0x00,         // set dclk: 80Hz:0xc1 90Hz:0xe1   100Hz:0x00   110Hz:0x30 120Hz:0x50   130Hz:0x70
	0xab, 0x01,         // Enable VDD 
	0xb6, 0x0f,         // Set Second Pre-charge period
	0xbe, 0x0f,         // set VCOMH Voltage
	0xbc, 0x08,         // Set pre-charge voltage level.
	0xd5, 0x62,         // Function selection B
	0xfd, 0x12,         // Set Command Lock (unlocked)
};

static uint8_t ssd1327_on = 0xaf;
static uint8_t ssd1327_off = 0xae;

struct device_context
{
	uint16_t fontwth;				// Current font width of selected font
	uint16_t fontht;				// Current font height of selected font
	uint16_t fontstride;			// Bytes between characters in font
	uint8_t* fontdata;				// Pointer to current font data	
	struct {
		uint16_t textcolor : 4;		// text color
		uint16_t bkgndcolor : 4;	// background colour
		uint16_t _reserved : 7;
		uint16_t inuse : 1;			// DC is in use
	};
};

#define MAX_DC ( 8 )
static struct device_context dc_table[MAX_DC] = { 0 };

typedef struct ssd1327_device
{
	SPI_HANDLE spi;				// SPI Handle for device
	uint16_t screenwth;			// Screen width
	uint16_t screenht;			// Screen ht
	GPIO_HANDLE gpio;			// GPIO handle for Data/Cmd access
	uint8_t data_cmd_gpio;		// GPIO number that is data/cmd pin
} SSD1327;

/* Global table of ssd1327 devices.  */
static SSD1327 tab[1] = { {0} };

/*-[ GetDc ]----------------------------------------------------------------}
.  Fetches the next available device context
.--------------------------------------------------------------------------*/
HDC GetDC (void)
{
	uint16_t i;
	for (i = 0; i < MAX_DC; i++)									// Search each table entry
	{
		if (dc_table[i].inuse == 0)									// Is DC in use
		{
			dc_table[i].bkgndcolor = 0;								// Default back ground colour
			dc_table[i].textcolor = 15;								// Default text colour
			dc_table[i].fontwth = 8;								// Default font width = 8
			dc_table[i].fontht = 16;								// Default font width = 16
			dc_table[i].fontstride = 16;							// Default font stride = 16 bytes per character
			dc_table[i].fontdata = (uint8_t*)&font_8x16_data[0];	// Default pointer to font data
			dc_table[i].inuse = 1;									// Set the in use flag
			return &dc_table[i];									// Return the handle
		}
	}
	return 0;														// No DC available
}

/*-[ ReleaseDc ]------------------------------------------------------------}
.  Releases the device context
.--------------------------------------------------------------------------*/
void ReleaseDC (HDC Dc)
{
	if (Dc && Dc->inuse)
	{
		Dc->inuse = 0;												// DC is available again
	}
}



void SelectFont (HDC Dc, uint8_t fontnum)
{
	if (Dc)
	{
		switch (fontnum)
		{
			case FONT8x8:
				Dc->fontdata = (uint8_t*)&font_8x8_data[0];
				Dc->fontht = 8;
				Dc->fontwth = 8;
				Dc->fontstride = 8;
				break;
			case FONT6x8:
				Dc->fontdata = (uint8_t*)&font_6x8_data[0];
				Dc->fontwth = 6;
				Dc->fontht = 8;
				Dc->fontstride = 8;
				break;
			default:
				Dc->fontdata = (uint8_t*)&font_8x16_data[0];
				Dc->fontwth = 8;
				Dc->fontht = 16;
				Dc->fontstride = 16;
		}
	}
}

/*-[ SSD1327_Open ]---------------------------------------------------------}
. Open access to an SSD1327 on the given SPI device handle. The SPI device
. should be opened with desired speed settings and SPI_MODE3 before call.
. It is also assumed a valid reset cycle on reset pin was completed.
. RETURN: true SSD1327 for success, false for any failure
.--------------------------------------------------------------------------*/
bool SSD1327_Open (SPI_HANDLE spi, GPIO_HANDLE gpio, uint8_t data_cmd_gpio)
{
	if (tab[0].spi == 0)											// Make sure device is not already open 
	{
		tab[0].spi = spi;											// Hold the SPI Handle
		tab[0].gpio = gpio;											// Hold GPIO handle
		tab[0].data_cmd_gpio = data_cmd_gpio;						// Hold gpio number for data_cmd
		tab[0].screenwth = 128;										// Set screen width
		tab[0].screenht = 128;										// Set screen height
		GPIO_Output(gpio, data_cmd_gpio, 0);						// Set to low .. ready for commands
		SpiWriteAndRead(spi, (uint8_t*)&ssd1327_init[0], 0, 34, false);// Send initialize commands
		return true;												// Return success
	}
	return false;
}

/*-[ SSD1327_ScreenOnOff ]--------------------------------------------------}
. Sends the command to turn the screen on/off.
.--------------------------------------------------------------------------*/
void SSD1327_ScreenOnOff (bool ScreenOn)
{
	GPIO_Output(tab[0].gpio, tab[0].data_cmd_gpio, 0);				// Data#Cmd low for command
	uint8_t* p = (ScreenOn) ? &ssd1327_on : &ssd1327_off;
	SpiWriteAndRead(tab[0].spi, p, 0, 1, false);					// Send off command commands
	GPIO_Output(tab[0].gpio, tab[0].data_cmd_gpio, 1);				// Data#Cmd back high
}


/*-[ SSD1327_ClearScreen ]--------------------------------------------------}
. Puts a colour on entrie screen
.--------------------------------------------------------------------------*/
void SSD1327_ClearScreen (uint8_t colour)
{
	unsigned int y;
	uint8_t buf[64];
	uint8_t temp = (colour << 4) | colour;
	memset(&buf[0],temp, 64); 
	GPIO_Output(tab[0].gpio, tab[0].data_cmd_gpio, 1);				// Make sure Data#Cmd high
        SSD1327_SetWindow(0, 0, 128, 128);
	for (y = 0; y < tab[0].screenht; y++)
	{
		SpiWriteAndRead(tab[0].spi, &buf[0], 0, 64, false);		// Write 2 pixels of color	
	}
}

/*-[ SSD1327_SetWindow ]----------------------------------------------------}
. Sets the window area to (x1,y1, x2, y2) so the next data commands are 
. into that area.
.--------------------------------------------------------------------------*/
void SSD1327_SetWindow (uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2)
{
	uint8_t temp[6];
	temp[0] = 0x15;
	temp[1] = x1/2;
	temp[2] = x2/2 - 1;
	temp[3] = 0x75;
	temp[4] = y1;
	temp[5] = y2 - 1;
	GPIO_Output(tab[0].gpio, tab[0].data_cmd_gpio, 0);			// Set to low .. ready for command
	SpiWriteAndRead(tab[0].spi, (uint8_t*)&temp[0], 0, 6, false);// Send set window command
	GPIO_Output(tab[0].gpio, tab[0].data_cmd_gpio, 1);			// Set to high data mode
}


/*-[ SSD1327_WriteChar ]----------------------------------------------------}
. Writes the character in the current font to the screen at position (x,y)
. **** Note x can only be even value 0,2,4 etc due to two pixel per byte 
. format and me being to lazy to buffer screen. If you do ask for an odd X
. value it will write at the value one less so x = 3 would write at x = 2.
. Fonts also have restriction they must be of even width .. the demo font 
. being 8 pixels wide.
.--------------------------------------------------------------------------*/
void SSD1327_WriteChar (HDC Dc, uint16_t x, uint16_t y, char Ch)
{
	if (tab[0].spi && Dc && Dc->fontdata)							// Make sure device is open and we have DC and fontdata
	{
		uint8_t* bp = &Dc->fontdata[(unsigned int)Ch * Dc->fontstride];// Load font bitmap pointer
		x &= 0xFFFE;												// Make sure x value even 											
		SSD1327_SetWindow(x, y, x + Dc->fontwth, y + Dc->fontht);
		for (uint16_t j = 0; j < Dc->fontht; j++)					// For each line in font height
		{
			uint8_t b = *bp++;										// Fetch the first font byte	
			for (uint16_t i = 0; i < Dc->fontwth/2; i++)			// We write two bytes at a time
			{
				uint8_t col;
				if ((b & 0x80) == 0x80) col = Dc->textcolor << 4;	// Set High pixel as text colour
					else col = Dc->bkgndcolor << 4;					// Set High pixel as back colour
				if ((b & 0x40) == 0x40) col |= Dc->textcolor;		// Set low pixel as text colour
					else col |= Dc->bkgndcolor;						// Set low pixel as back colour
				SpiWriteAndRead(tab[0].spi, (uint8_t*)&col, 0, 1, false);// Send col as data			
				b  = b << 2;										// Shift 2 bit left
				if ((((i + 1) % 4) == 0) && ((i + 1) < Dc->fontwth/2))
				{
					b = *bp++;										// Load next font byte
				}
			}

		}
	}
}

/*-[ SSD1327_WriteText ]----------------------------------------------------}
. Writes the text in the current font to the screen at position (x,y)
. **** Note x can only be even value 0,2,4 etc due to two pixel per byte
. format and me being to lazy to buffer screen. If you do ask for an odd X
. value it will write at the value one less so x = 3 would write at x = 2.
. Fonts also have restriction they must be of even width .. the demo font
. being 8 pixels wide.
.--------------------------------------------------------------------------*/
void SSD1327_WriteText (HDC Dc, uint16_t x, uint16_t y, char* txt)
{
	if (tab[0].spi && Dc && Dc->fontdata && txt)					// Make sure device is open and we have fontdata and txt pointer
	{
		x &= 0xFFFE;												// Make sure x value even 
		while ((*txt) != 0)											// Not a c string terminate character
		{
			char ch = (*txt++);										// Next character
			SSD1327_WriteChar(Dc, x, y, ch);						// Write the charter to screen
			x += Dc->fontwth;										// Move to next character position
		}
	}
}
