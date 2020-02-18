/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++}
{																			}
{       Filename: ssd1327.c													}
{       Copyright(c): Leon de Boer(LdB) 2020								}
{       Version: 1.10														}
{		Release under MIT license (https://opensource.org/licenses/MIT)     }
{																			}
{***************************************************************************}
{                                                                           }
{      Defines an API interface for the SSD1327 SPI controller				}
{																            }
{++++++++++++++++++++++++[ REVISIONS ]++++++++++++++++++++++++++++++++++++++}
{  1.00 Initial version														}
{  1.10 Added device context and more primitives							}
{++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/

#include <stdbool.h>							// C standard unit for bool, true, false
#include <stdint.h>								// C standard unit for uint32_t etc
#include <string.h>								// C standard unit needed for memset
#include "spi.h"								// SPI device unit as we will be using SPI
#include "gpio.h"								// We need access to GPIO to resetup reset pin
#include "font8x16.h"							// Font 16x8 bitmap data
#include "font8x8.h"							// Font 8x8 bitmap data
#include "font6x8.h"							// Font 6x8 bitmap data
#include "ssd1327.h"							// This units header

#if SSD1327_DRIVER_VERSION != 1100
#error "Header does not match this version of file"
#endif

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
		uint32_t hiTxtColor : 8;	// 8 bits to use for high pixel of text colour
		uint32_t loTxtColor : 8;	// 8 bits to use for low pixel of text colour
		uint32_t hiBkColor : 8;		// 8 bits to use for high pixel of background colour
		uint32_t loBkColor : 8;		// 8 bits to use for low pixel of background colour
	};
	struct {
		uint32_t hiBrushColor : 8;	// 8 bits to use for high pixel of brush colour
		uint32_t loBrushColor : 8;	// 8 bits to use for low pixel of brush colour
		uint32_t hiPenColor : 8;	// 8 bits to use for high pixel of pen colour
		uint32_t loPenColor : 8;	// 8 bits to use for low pixel of pen colour
	};
	struct {
		uint16_t curfontnum : 7;	// Current selected font number
		uint16_t _reserved : 8;
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
	uint8_t data_cmd_gpio;		// GPIO number that is Data/Cmd pin
} SSD1327;

/* Global table of ssd1327 devices.  */
static SSD1327 tab[1] = { {0} };

/***************************************************************************}
{						 DEVICE SPECIFIC ROUTINES	                        }
{***************************************************************************/

/*-[ SSD1327_Open ]---------------------------------------------------------}
. Open access to an SSD1327 on the given SPI device handle. The SPI device
. should be opened with desired speed settings and SPI_MODE3 before call.
. It is also assumed a valid reset cycle on reset pin was completed and the
. reset operation lies outside this code scope as it involves long delays.
. RETURN: true for success, false for any failure
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
		GPIO_Output(tab[0].gpio, tab[0].data_cmd_gpio, 1);			// Data#Cmd back high for safety
		return true;												// Return success
	}
	return false;
}

/*-[ SSD1327_ScreenOnOff ]--------------------------------------------------}
. Sends the command to turn the screen on/off.
. RETURN: true for success, false for any failure
.--------------------------------------------------------------------------*/
bool SSD1327_ScreenOnOff (bool ScreenOn)
{
	GPIO_Output(tab[0].gpio, tab[0].data_cmd_gpio, 0);				// Data#Cmd low for command
	uint8_t* p = (ScreenOn) ? &ssd1327_on : &ssd1327_off;
	bool retVal = SpiWriteAndRead(tab[0].spi, p, 0, 1, false);		// Send off command commands
	GPIO_Output(tab[0].gpio, tab[0].data_cmd_gpio, 1);				// Data#Cmd back high for safety
	return retVal;													// Return result of transmission
}

/*-[ SSD1327_SetWindow ]----------------------------------------------------}
. Sets the window area to (x1,y1, x2, y2) so the next data commands are
. into that area.
. RETURN: true for success, false for any failure
.--------------------------------------------------------------------------*/
bool SSD1327_SetWindow (uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2)
{
	uint8_t temp[6];
	temp[0] = 0x15;
	temp[1] = x1 / 2;
	temp[2] = x2 / 2 - 1;
	temp[3] = 0x75;
	temp[4] = y1;
	temp[5] = y2 - 1;
	GPIO_Output(tab[0].gpio, tab[0].data_cmd_gpio, 0);				// Set to low .. ready for command
	bool retVal = SpiWriteAndRead(tab[0].spi, (uint8_t*)&temp[0], 
		0, 6, false);												// Send set window command
	GPIO_Output(tab[0].gpio, tab[0].data_cmd_gpio, 1);				// Data#Cmd back high for safety
	return retVal;													// Return result of transmission
}

/*-[ SSD1327_ClearScreen ]--------------------------------------------------}
. Puts a colour on entire screen
. RETURN: true for success, false for any failure
.--------------------------------------------------------------------------*/
bool SSD1327_ClearScreen (uint8_t colour)
{

	uint8_t buf[tab[0].screenwth / 2];								// Setup a buffer for a single line
	uint8_t temp = (colour << 4) | colour;							// Create a single colour byte of 2 pixels
	memset(&buf[0], temp, tab[0].screenwth / 2);					// Fill the temp buffer with the colour
	if (SSD1327_SetWindow(0, 0, tab[0].screenwth, tab[0].screenht))	// Set the window to entire screen
	{
		GPIO_Output(tab[0].gpio, tab[0].data_cmd_gpio, 1);			// Make sure Data#Cmd high
		return (SpiWriteBlockRepeat(tab[0].spi, &buf[0],
			tab[0].screenwth / 2, tab[0].screenht, false));			// Transfer buffer repeatedly and return result
	}
	return false;													// Set window failed
}

/*-[ SSD1327_WriteChar ]----------------------------------------------------}
. Writes the character in the current font to the screen at position (x,y)
. **** Note x can only be even value 0,2,4 etc due to two pixel per byte 
. format and me being to lazy to deal with odds. If you do ask for an odd X
. value it will write at the value one less so x = 3 would write at x = 2.
. Fonts also have restriction they must be of even width .. the demo fonts 
. being 6 and 8 pixels wide.
. RETURN: true for success, false for any failure
.--------------------------------------------------------------------------*/
bool SSD1327_WriteChar (HDC Dc, uint16_t x, uint16_t y, char Ch)
{
	if (tab[0].spi && Dc && Dc->fontdata)							// Make sure device is open and we have DC and fontdata
	{
		uint16_t TSize = Dc->fontwth/2 * Dc->fontht;				// Bytes to tranfer for font is Fontwidth/2 * FontHt
		uint8_t* bp = &Dc->fontdata[(unsigned int)Ch * Dc->fontstride];// Load font bitmap pointer
		x &= 0xFFFE;												// Make sure x value even 											
		if (SSD1327_SetWindow(x, y, x + Dc->fontwth, y + Dc->fontht))// Set the window area
		{	
			uint8_t buf[TSize];										// Setup a buffer for transfer 
			uint8_t b = *bp++;										// Fetch the first font byte
			uint8_t fbuc = 0;										// Zero font bits used count for fonts > 8 pixels in width
			for (unsigned int i = 0; i < TSize; i++)
			{
				buf[i] = ((b & 0x80) == 0x80) ? Dc->hiTxtColor : Dc->hiBkColor; // High pixel colour either text or bkgnd
				buf[i] |= ((b & 0x40) == 0x40) ? Dc->loTxtColor : Dc->loBkColor; // Low pixel colour either text or bkgnd
				b = b << 2;											// Shift b left by two places
				fbuc++;												// Increment font bits used count
				if ((i + 1) % (Dc->fontwth/2) == 0 || fbuc == 4)	// If the byte is a mod of fontwth/2 or we have used all 8 font bits
				{
					b = *bp++;										// Load next byte from font
					fbuc = 0;										// Zero font bits used as we ahve new font byte 
				}
			}
			GPIO_Output(tab[0].gpio, tab[0].data_cmd_gpio, 1);		// Make sure Data#Cmd high
			return SpiWriteAndRead(tab[0].spi, &buf[0], 0, TSize, false);// Send all font data
		}
	}
	return false;													// Return failure
}

/*-[ SSD1327_WriteText ]----------------------------------------------------}
. Writes the text in the current font to the screen at position (x,y)
. **** Note x can only be even value 0,2,4 etc due to two pixel per byte
. format and me being to lazy to deal with odds. If you do ask for an odd X
. value it will write at the value one less so x = 3 would write at x = 2.
. Fonts also have restriction they must be of even width .. the demo font
. being 8 pixels wide.
. RETURN: true for success, false for any failure
.--------------------------------------------------------------------------*/
bool SSD1327_WriteText (HDC Dc, uint16_t x, uint16_t y, char* txt)
{
	if (tab[0].spi && Dc && Dc->fontdata && txt)					// Make sure device is open and we have fontdata and txt pointer
	{
		bool retVal = true;											// Preset success
		x &= 0xFFFE;												// Make sure x value even 
		while ((*txt) != 0 && retVal)								// Not a c string terminate character and retVal still true
		{
			char ch = (*txt++);										// Next character
			retVal = SSD1327_WriteChar(Dc, x, y, ch);				// Write the charter to screen
			x += Dc->fontwth;										// Move to next character position
		}
		return retVal;												// Return result
	}
	return false;													// Return failure
}

/***************************************************************************}
{						 DEVICE CONTEXT ROUTINES	                        }
{***************************************************************************/

/*-[ GetDc ]----------------------------------------------------------------}
.  Fetches the next available device context handle (HDC).
. If all handles are currently in use it will return NULL.
.--------------------------------------------------------------------------*/
HDC GetDC (void)
{
	uint16_t i;
	for (i = 0; i < MAX_DC; i++)									// Search each table entry
	{
		if (dc_table[i].inuse == 0)									// Is DC in use
		{
			dc_table[i].inuse = 1;									// Set the in use flag
			SetBkColor(&dc_table[i], 0);							// Set background colour black
			SetTextColor(&dc_table[i], 15);							// Set text colour white
			SetDCBrushColor(&dc_table[i], 8);						// Set brush colour mid gray
			SetDCPenColor(&dc_table[i], 8);							// Set pen colour mid gray
			dc_table[i].fontwth = 8;								// Default font width = 8
			dc_table[i].fontht = 16;								// Default font width = 16
			dc_table[i].fontstride = 16;							// Default font stride = 16 bytes per character
			dc_table[i].fontdata = (uint8_t*)&font_8x16_data[0];	// Default pointer to font data
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
	if (Dc && Dc->inuse)											// Check the DC is valid and in use
	{
		Dc->inuse = 0;												// DC is available again
	}
}

/*-[ SetBkColor ]-----------------------------------------------------------}
.  Sets the current device context background color to the specified color 
.  and returns the previously set background color
.--------------------------------------------------------------------------*/
COLORREF SetBkColor(HDC Dc, COLORREF Color)
{
	COLORREF retVal = 0;											// Preset zero return
	if (Dc && Dc->inuse)											// Check the DC is valid and in use
	{
		retVal = Dc->loBkColor;										// Return current low pixel background colour
		Dc->hiBkColor = Color << 4;									// Set the high pixel background colour 
		Dc->loBkColor = Color & 0xF;								// Set the low pixel background colour
	}
	return retVal;													// Return the old background colour
}

/*-[ SetTextColor ]---------------------------------------------------------}
.  Sets the current device context text color to the specified color and 
.  returns the previously set text color
.--------------------------------------------------------------------------*/
COLORREF SetTextColor (HDC Dc, COLORREF Color)
{
	COLORREF retVal = 0;											// Preset zero return
	if (Dc && Dc->inuse)											// Check the DC is valid and in use
	{
		retVal = Dc->loTxtColor;									// Return current low pixel text colour
		Dc->hiTxtColor = Color << 4;								// Set the high pixel text colour 
		Dc->loTxtColor = Color & 0xF;								// Set the low pixel text colour
	}
	return retVal;													// Return the old text colour
}

/*-[ SetDCPenColor ]--------------------------------------------------------}
.  Sets the current device context pen color to the specified color and
. returns the previously set pen color
.--------------------------------------------------------------------------*/
COLORREF SetDCPenColor (HDC Dc, COLORREF Color)
{
	COLORREF retVal = 0;											// Preset zero return
	if (Dc && Dc->inuse)											// Check the DC is valid and in use
	{
		retVal = Dc->loPenColor;									// Return current low pixel pen colour
		Dc->hiPenColor = Color << 4;								// Set the high pixel pen colour 
		Dc->loPenColor = Color & 0xF;								// Set the low pixel pen colour
	}
	return retVal;													// Return the old pen colour
}

/*-[ SetDCBrushColor ]------------------------------------------------------}
.  Sets the current device context brush color to the specified color and 
. returns the previously set brush color
.--------------------------------------------------------------------------*/
COLORREF SetDCBrushColor (HDC Dc, COLORREF Color)
{
	COLORREF retVal = 0;											// Preset zero return
	if (Dc && Dc->inuse)											// Check the DC is valid and in use
	{
		retVal = Dc->loBrushColor;									// Return current low pixel brush colour
		Dc->hiBrushColor = Color << 4;								// Set the high pixel brush colour 
		Dc->loBrushColor = Color & 0xF;								// Set the low pixel brush colour
	}
	return retVal;													// Return the old brush colour
}

/*-[ Rectangle ]------------------------------------------------------------}
. Draws a rectangle using the current brush color.
. RETURN: true for success, false for any failure
.--------------------------------------------------------------------------*/
bool Rectangle (HDC Dc, uint16_t left, uint16_t top, uint16_t right, uint16_t bottom)
{
	if (Dc && Dc->inuse)											// Check the DC is valid
	{
		if (left > tab[0].screenwth) left = tab[0].screenwth;		// Make sure left is in screen area
		if (right > tab[0].screenwth) right = tab[0].screenwth;		// Make sure right is in screen area
		if (top > tab[0].screenht) top = tab[0].screenht;			// Make sure top is in screen area
		if (bottom > tab[0].screenht) bottom = tab[0].screenht;		// Make sure top is in screen area
		if (left < right && top < bottom)							// Make sure left < right and top < bottom
		{
			uint8_t buf[(right - left) / 2];						// Setup a buffer for a single line
			memset(&buf[0], Dc->hiBrushColor | Dc->loBrushColor, 
				(right - left) / 2);								// Fill the temp buffer with the brush colour
			if (SSD1327_SetWindow(left, top, right, bottom))		// Set the window
			{
				GPIO_Output(tab[0].gpio, tab[0].data_cmd_gpio, 1);	// Make sure Data#Cmd high
				return (SpiWriteBlockRepeat(tab[0].spi, &buf[0],
					(right - left) / 2, bottom - top, false));		// Transfer buffer repeatedly and return result
			}
		}
	}
	return false;													// Return error
}

/*-[ SelectFont ]-----------------------------------------------------------}
. Set the current font on the device context to the specified font and
. returns the previosuly selected font.
.--------------------------------------------------------------------------*/
uint8_t SelectFont(HDC Dc, uint8_t fontnum)
{
	uint8_t retVal = 0;												// Preset zero return
	if (Dc)
	{
		retVal = Dc->curfontnum;									// Return will be current font number
		switch (fontnum)
		{
		case FONT8x8:
			Dc->fontdata = (uint8_t*)&font_8x8_data[0];				// Set 8x8 font data pointer
			Dc->fontwth = 8;										// Font width is 8 pixels
			Dc->fontht = 8;											// Font height is 8 pixels
			Dc->fontstride = 8;										// Font stride is 8 bytes
			Dc->curfontnum = FONT8x8;								// Set current font number
			break;
		case FONT6x8:
			Dc->fontdata = (uint8_t*)&font_6x8_data[0];				// Set 6x8 font data pointer
			Dc->fontwth = 6;										// Font width is 6 pixels
			Dc->fontht = 8;											// Font height is 8 pixels
			Dc->fontstride = 8;										// Font stride is 8 bytes
			Dc->curfontnum = FONT6x8;								// Set current font number
			break;
		default:
			Dc->fontdata = (uint8_t*)&font_8x16_data[0];			// Set 8x16 font data pointer
			Dc->fontwth = 8;										// Font width is 8 pixels
			Dc->fontht = 16;										// Font height is 8 pixels
			Dc->fontstride = 16;									// Font stride is 16 bytes
			Dc->curfontnum = FONT8x16;								// Set current font number
		}
	}
	return retVal;													// Return previous font number 
}
