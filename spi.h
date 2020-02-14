#ifndef _SPI_H_
#define _SPI_H_

#ifdef __cplusplus								// If we are including to a C++
extern "C" {									// Put extern C directive wrapper around
#endif

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++}
{																			}
{       Filename: spi.h														}
{       Copyright(c): Leon de Boer(LdB) 2019, 2020							}
{       Version: 1.10														}
{																			}
{***************************************************************************}
{                                                                           }
{     Defines an API interface for the SPI devices on Linux					}
{																            }
{++++++++++++++++++++++++[ REVISIONS ]++++++++++++++++++++++++++++++++++++++}
{  1.00 Initial version														}
{  1.10 Compacted stuct fields  											}
{++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/

#include <stdbool.h>							// C standard unit for bool, true, false
#include <stdint.h>								// C standard unit for uint32_t etc

#define SPI_DRIVER_VERSION 1100					// Version number 1.10 build 0

typedef enum {
   SPI_CS_Mode_LOW = 0,             /*!< Chip Select 0 */
   SPI_CS_Mode_HIGH = 1,            /*!< Chip Select 1 */
   SPI_CS_Mode_NONE = 2             /*!< No CS, control it yourself */
} SPIChipSelect;

typedef enum
{
    SPI_BIT_ORDER_LSBFIRST = 0,     /*!< LSB First */
    SPI_BIT_ORDER_MSBFIRST = 1      /*!< MSB First */
} SPIBitOrder;

typedef struct spi_device* SPI_HANDLE;			// Define an SPI_HANDLE pointer to opaque internal struct

#define NSPI 2									// 2 SPI devices supported


/*-[ SpiOpenPort ]----------------------------------------------------------}
. Creates a SPI handle which provides access to the SPI device number.
. The SPI device is setup to the bits, speed and mode provided.
. RETURN: valid SPI_HANDLE for success, NULL for any failure
.--------------------------------------------------------------------------*/
SPI_HANDLE SpiOpenPort (uint8_t spi_devicenum, uint8_t bit_exchange_size, uint32_t speed, uint8_t mode, bool useLock);

/*-[ SpiClosePort ]---------------------------------------------------------}
. Given a valid SPI handle the access is released and the handle freed.
. RETURN: true for success, false for any failure
.--------------------------------------------------------------------------*/
bool SpiClosePort (SPI_HANDLE spiHandle);

/*-[ SpiSetMode ]-----------------------------------------------------------}
. Given a valid SPI handle sets the SPI mode to that given.
. RETURN: true for success, false for any failure
.--------------------------------------------------------------------------*/
bool SpiSetMode (SPI_HANDLE spiHandle, uint16_t mode);

/*-[ SpiSetSpeed ]----------------------------------------------------------}
. Given a valid SPI handle sets the SPI read and write speed.
. RETURN: true for success, false for any failure
.--------------------------------------------------------------------------*/
bool SpiSetSpeed (SPI_HANDLE spiHandle, uint32_t speed);

/*-[ SpiSetChipSelect ]-----------------------------------------------------}
. Given a valid SPI handle sets the SPI chip select mode to that given.
. RETURN: true for success, false for any failure
.--------------------------------------------------------------------------*/
bool SpiSetChipSelect (SPI_HANDLE spiHandle, SPIChipSelect CS_Mode);

/*-[ SpiSetBitOrder ]-----------------------------------------------------}
. Given a valid SPI handle sets the SPI bit order(LSB/MSB) to that given.
. RETURN: true for success, false for any failure
.--------------------------------------------------------------------------*/
bool SpiSetBitOrder(SPI_HANDLE spiHandle, SPIBitOrder Order);

/*-[ SpiSetBitsPerWord ]----------------------------------------------------}
. Given a valid SPI handle sets the SPI bits per transmission word.
. RETURN: true for success, false for any failure
.--------------------------------------------------------------------------*/
bool SpiSetBitsPerWord (SPI_HANDLE spiHandle, uint8_t bits);

/*-[ SpiWriteAndRead ]------------------------------------------------------}
. Given a valid SPI handle and valid data pointers the call will send and
. receive data to and from the buffer pointers. As the write occurs before
. the read the buffer pointers can be the same buffer space.
. RETURN: >= 0 transfer count for success, < 0 for any error
.--------------------------------------------------------------------------*/
int SpiWriteAndRead (SPI_HANDLE spiHandle, uint8_t* TxData, uint8_t* RxData, uint16_t Length, bool LeaveCsLow);

/*-[ SpiWriteBlockRepeat ]--------------------------------------------------}
. Given a valid SPI handle and valid data pointers the call will send the
. data block count times. It is used to speed up things like writing LCD
. SPI screen areas a fixed colour.
. RETURN: >= 0 blocks transfered for success, < 0 for any error  
.--------------------------------------------------------------------------*/
int SpiWriteBlockRepeat (SPI_HANDLE spiHandle, uint8_t* TxBlock, uint16_t TxBlockLen, uint32_t Repeats, bool LeaveCsLow);

#ifdef __cplusplus								// If we are including to a C++ file
}												// Close the extern C directive wrapper
#endif

#endif