#ifndef _SSD1327_H_
#define _SSD1327_H_

#ifdef __cplusplus								// If we are including to a C++
extern "C" {									// Put extern C directive wrapper around
#endif

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++}
{																			}
{       Filename: ssd1327.h													}
{       Copyright: Leon de Boer(LdB) 2020									}
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
#include "spi.h"								// SPI device unit as we will be using SPI

#define SSD1327_DRIVER_VERSION 1100				// Version number 1.10 build 0

#define FONT8x16	( 0 )
#define FONT8x8		( 1 )
#define FONT6x8		( 2 )

/*--------------------------------------------------------------------------}
{						 COLORREF defined as a byte							}
{--------------------------------------------------------------------------*/
typedef uint8_t COLORREF;

/*--------------------------------------------------------------------------}
{     HDC is an opaque struct ptr the caller does not need to know about    }
{--------------------------------------------------------------------------*/
typedef struct device_context* HDC;

/***************************************************************************}
{						 DEVICE SPECIFIC ROUTINES	                        }
{***************************************************************************/

/*-[ SSD1327_Open ]---------------------------------------------------------}
. Open access to an SSD1327 on the given SPI device handle. The SPI device
. should be opened with desired speed settings and SPI_MODE3 before call.
. It is also assumed a valid reset cycle on reset pin was completed and the
. reset operation lies outside this code scope as it involves long delays.
. RETURN: valid SSD1327 for success, NULL for any failure
.--------------------------------------------------------------------------*/
bool SSD1327_Open (SPI_HANDLE spi, GPIO_HANDLE gpio, uint8_t data_cmd_gpio);

/*-[ SSD1327_ScreenOnOff ]--------------------------------------------------}
. Sends the command to turn the screen on/off.
. RETURN: true for success, false for any failure
.--------------------------------------------------------------------------*/
bool SSD1327_ScreenOnOff (bool ScreenOn);

/*-[ SSD1327_SetWindow ]----------------------------------------------------}
. Sets the window area to (x1,y1, x2, y2) so the next data commands are
. into that area.
.--------------------------------------------------------------------------*/
bool SSD1327_SetWindow (uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2);

/*-[ SSD1327_ClearScreen ]--------------------------------------------------}
. Puts a colour on entire screen
. RETURN: true for success, false for any failure
.--------------------------------------------------------------------------*/
bool SSD1327_ClearScreen (uint8_t colour);

/*-[ SSD1327_WriteChar ]----------------------------------------------------}
. Writes the character in the current font to the screen at position (x,y)
. **** Note x can only be even value 0,2,4 etc due to two pixel per byte
. format and me being to lazy to deal with odds. If you do ask for an odd X
. value it will write at the value one less so x = 3 would write at x = 2.
. Fonts also have restriction they must be of even width .. the demo fonts
. being 6 and 8 pixels wide.
. RETURN: true for success, false for any failure
.--------------------------------------------------------------------------*/
bool SSD1327_WriteChar (HDC Dc, uint16_t x, uint16_t y, char Ch);

/*-[ SSD1327_WriteText ]----------------------------------------------------}
. Writes the text in the current font to the screen at position (x,y)
. **** Note x can only be even value 0,2,4 etc due to two pixel per byte
. format and me being to lazy to deal with odds. If you do ask for an odd X
. value it will write at the value one less so x = 3 would write at x = 2.
. Fonts also have restriction they must be of even width .. the demo font
. being 8 pixels wide.
. RETURN: true for success, false for any failure
.--------------------------------------------------------------------------*/
bool SSD1327_WriteText (HDC Dc, uint16_t x, uint16_t y, char* txt);

/***************************************************************************}
{						 DEVICE CONTEXT ROUTINES	                        }
{***************************************************************************/

/*-[ GetDc ]----------------------------------------------------------------}
. Fetches the next available device context handle (HDC).
. If all handles are currently in use it will return NULL.
.--------------------------------------------------------------------------*/
HDC GetDC (void);

/*-[ ReleaseDc ]------------------------------------------------------------}
.  Releases the device context
.--------------------------------------------------------------------------*/
void ReleaseDC (HDC Dc);

/*-[ SetBkColor ]-----------------------------------------------------------}
. Sets the current device context background color to the specified color
. and returns the previously set background color
.--------------------------------------------------------------------------*/
COLORREF SetBkColor (HDC Dc, COLORREF Color);

/*-[ SetTextColor ]---------------------------------------------------------}
. Sets the current device context text color to the specified color and
. returns the previously set text color
.--------------------------------------------------------------------------*/
COLORREF SetTextColor (HDC Dc, COLORREF Color);

/*-[ SetDCPenColor ]--------------------------------------------------------}
. Sets the current device context pen color to the specified color and
. returns the previously set pen color
.--------------------------------------------------------------------------*/
COLORREF SetDCPenColor (HDC Dc, COLORREF Color);

/*-[ SetDCBrushColor ]------------------------------------------------------}
. Sets the current device context brush color to the specified color and
. returns the previously set brush color
.--------------------------------------------------------------------------*/
COLORREF SetDCBrushColor (HDC Dc, COLORREF Color);

/*-[ Rectangle ]------------------------------------------------------------}
. Draws a rectangle using the current brush color.
. RETURN: true for success, false for any failure
.--------------------------------------------------------------------------*/
bool Rectangle(HDC Dc, uint16_t left, uint16_t top, uint16_t right, uint16_t bottom);

/*-[ SelectFont ]-----------------------------------------------------------}
. Set the current font on the device context to the specified font and
. returns the previosuly selected font.
.--------------------------------------------------------------------------*/
uint8_t SelectFont (HDC Dc, uint8_t fontnum);

#ifdef __cplusplus								// If we are including to a C++ file
}												// Close the extern C directive wrapper
#endif

#endif
