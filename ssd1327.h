#ifndef _SSD1327_H_
#define _SSD1327_H_

#ifdef __cplusplus								// If we are including to a C++
extern "C" {									// Put extern C directive wrapper around
#endif

#include <stdbool.h>							// C standard unit for bool, true, false
#include <stdint.h>								// C standard unit for uint32_t etc
#include "spi.h"								// SPI device unit as we will be using SPI

#define FONT8x16	( 0 )
#define FONT8x8		( 1 )
#define FONT6x8		( 2 )

typedef struct device_context* HDC;

/*-[ SSD1327_Open ]---------------------------------------------------------}
. Open access to an SSD1327 on the given SPI device handle. The SPI device
. should be opened with desired speed settings and SPI_MODE3 before call.
. It is also assumed a valid reset cycle on reset pin was completed.
. RETURN: valid SSD1327 for success, NULL for any failure
.--------------------------------------------------------------------------*/
bool SSD1327_Open (SPI_HANDLE spi, GPIO_HANDLE gpio, uint8_t data_cmd_gpio);

/*-[ SSD1327_ScreenOnOff ]--------------------------------------------------}
. Sends the command to turn the screen on/off.
.--------------------------------------------------------------------------*/
void SSD1327_ScreenOnOff(bool ScreenOn);


/*-[ SSD1327_ClearScreen ]--------------------------------------------------}
. Puts a colour on entire screen
.--------------------------------------------------------------------------*/
void SSD1327_ClearScreen (uint8_t colour);

/*-[ SSD1327_SetWindow ]----------------------------------------------------}
. Sets the window area to (x1,y1, x2, y2) so the next data commands are
. into that area.
.--------------------------------------------------------------------------*/
void SSD1327_SetWindow (uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2);

/*-[ SSD1327_WriteChar ]----------------------------------------------------}
. Writes the character in the current font to the screen at position (x,y)
. **** Note x can only be even value 0,2,4 etc due to two pixel per byte
. format and me being to lazy to buffer screen. If you do ask for an odd X
. value it will write at the value one less so x = 3 would write at x = 2.
. Fonts also have restriction they must be of even width .. the demo font
. being 8 pixels wide.
.--------------------------------------------------------------------------*/
void SSD1327_WriteChar (HDC Dc, uint16_t x, uint16_t y, char Ch);

/*-[ SSD1327_WriteText ]----------------------------------------------------}
. Writes the text in the current font to the screen at position (x,y)
. **** Note x can only be even value 0,2,4 etc due to two pixel per byte
. format and me being to lazy to buffer screen. If you do ask for an odd X
. value it will write at the value one less so x = 3 would write at x = 2.
. Fonts also have restriction they must be of even width .. the demo font
. being 8 pixels wide.
.--------------------------------------------------------------------------*/
void SSD1327_WriteText (HDC Dc, uint16_t x, uint16_t y, char* txt);


/*-[ GetDc ]----------------------------------------------------------------}
.  Fetches the next available device context
.--------------------------------------------------------------------------*/
HDC GetDC (void);

/*-[ ReleaseDc ]------------------------------------------------------------}
.  Releases the device context
.--------------------------------------------------------------------------*/
void ReleaseDC (HDC Dc);


void SelectFont(HDC Dc, uint8_t fontnum);

#ifdef __cplusplus								// If we are including to a C++ file
}												// Close the extern C directive wrapper
#endif

#endif
