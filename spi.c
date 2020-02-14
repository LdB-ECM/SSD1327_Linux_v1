/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++}
{																			}
{       Filename: spi.c														}
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

#include <stdbool.h>			// C standard unit for bool, true, false
#include <stdint.h>				// C standard unit for uint32_t etc
#include <fcntl.h>				// Needed for SPI port
#include <sys/ioctl.h>			// Needed for SPI port
#include <linux/spi/spidev.h>	// Needed for SPI port
#include <unistd.h>				// Needed for SPI port
#include <stdio.h>				// neded for sprintf_s
#include <pthread.h>			// Posix thread unit
#include <semaphore.h>			// Linux Semaphore unit
#include "spi.h"				// This units header

#if SPI_DRIVER_VERSION != 1100
#error "Header does not match this version of file"
#endif

#define SPI_CS_HIGH     0x04					// Chip select high  
#define SPI_LSB_FIRST   0x08					// LSB  
#define SPI_3WIRE       0x10					// 3-wire mode SI and SO same line
#define SPI_LOOP        0x20					// Loopback mode  
#define SPI_NO_CS       0x40					// A single device occupies one SPI bus, so there is no chip select 
#define SPI_READY       0x80					// Slave pull low to stop data transmission  

struct spi_device
{
	int spi_fd;									// File descriptor for the SPI device
	uint32_t spi_speed;							// SPI speed
	uint16_t mode;								// SPI mode bits
    sem_t lock;									// Semaphore for lock
	struct {
        uint16_t spi_bitsPerWord: 8;			// SPI bits per word
        uint16_t spi_num : 4;					// SPI device table number
        uint16_t _reserved: 2;        			// reserved
		uint16_t uselocks : 1;					// Locks to be used for access
		uint16_t inuse : 1;						// In use flag
	};
};

/* Global table of SPI devices.  */
static struct spi_device spitab[NSPI] = { {0} };

/*-[ SpiOpenPort ]----------------------------------------------------------}
. Creates a SPI handle which provides access to the SPI device number.
. The SPI device is setup to the bits, speed and mode provided.
. RETURN: valid SPI_HANDLE for success, NULL for any failure
.--------------------------------------------------------------------------*/
SPI_HANDLE SpiOpenPort (uint8_t spi_devicenum, uint8_t bit_exchange_size, uint32_t speed, uint8_t mode, bool useLock)
{
	SPI_HANDLE spi = 0;												// Preset null handle
	struct spi_device* spi_ptr = &spitab[spi_devicenum];			// SPI device pointer 
	if (spi_devicenum < NSPI && spi_ptr->inuse == 0 && speed != 0)
	{
		spi_ptr->spi_fd = 0;										// Zero SPI file device
		spi_ptr->spi_num = spi_devicenum;							// Hold spi device number
		spi_ptr->uselocks = (useLock == true) ? 1 : 0;				// Set use lock
        if (useLock)                                                // Using locks
        {
			sem_init(&spi_ptr->lock, 0, 1);							// Initialize mutex to 1
        }
        char buf[256] = { 0 };
		sprintf(&buf[0], "/dev/spidev0.%c", (char)(0x30 + spi_devicenum));
		int fd = open(&buf[0], O_RDWR);								// Open the SPI device
		if (fd >= 0)												// SPI device opened correctly
		{
                        spi_ptr->inuse =  1;
			spi_ptr->spi_fd = fd;									// Hold the file device to SPI
			if (SpiSetMode(spi_ptr, mode) &&						// Set spi mode
				SpiSetBitsPerWord(spi_ptr, bit_exchange_size) &&	// Set spi bits per exchange
				SpiSetSpeed(spi_ptr, speed) &&						// Set spi speed
				SpiSetBitOrder(spi_ptr, SPI_BIT_ORDER_MSBFIRST) &&  // Set spi MSB bit order
				SpiSetChipSelect(spi_ptr, SPI_CS_Mode_LOW))			// Set SPI chip select low
			{
				spi = spi_ptr;										// Return SPI handle
			} else spi_ptr-> inuse = 0;
		}
	}
	return(spi);													// Return SPI handle result			
}

/*-[ SpiClosePort ]---------------------------------------------------------}
. Given a valid SPI handle the access is released and the handle freed.
. RETURN: true for success, false for any failure
.--------------------------------------------------------------------------*/
bool SpiClosePort (SPI_HANDLE spiHandle)
{
	if (spiHandle && spiHandle->inuse)								// SPI handle valid and SPI handle is in use
	{
		if (spiHandle->uselocks)									// Using locks
		{
			sem_destroy(&spiHandle->lock);							// Destroy lock mutex
		}
		close(spiHandle->spi_fd);									// Close the spi handle
		spiHandle->spi_fd = 0;										// Zero the SPI handle
		spiHandle->spi_num = 0;										// Zero SPI handle number
		spiHandle->inuse = 0;										// The SPI handle is now free
		return true;												// Return success
	}
	return false;													// Return failure
}

#define all_mode_bits  (SPI_MODE_0 | SPI_MODE_1 | SPI_MODE_2 | SPI_MODE_3 )
/*-[ SpiSetMode ]-----------------------------------------------------------}
. Given a valid SPI handle sets the SPI mode to that given.
. RETURN: true for success, false for any failure
.--------------------------------------------------------------------------*/
bool SpiSetMode (SPI_HANDLE spiHandle, uint16_t mode)
{
	if (spiHandle && spiHandle->inuse)								// SPI handle valid and SPI handle is in use
	{	
		mode &= all_mode_bits;										// Ensures mode is only valid mod bits
		spiHandle->mode &= ~all_mode_bits;							// Clear all existing mode bits
		spiHandle->mode |= mode;									// Set requested mode bits
		if (ioctl(spiHandle->spi_fd, SPI_IOC_WR_MODE, &spiHandle->mode) >= 0)
		{
			return true;											// Return success
		}
	}
	return false;													// Return failure
}

/*-[ SpiSetSpeed ]----------------------------------------------------------}
. Given a valid SPI handle sets the SPI read and write speed.
. RETURN: true for success, false for any failure
.--------------------------------------------------------------------------*/
bool SpiSetSpeed (SPI_HANDLE spiHandle, uint32_t speed)
{
	if (spiHandle && spiHandle->inuse &&  speed > 0)				// SPI handle valid and SPI handle is in use
	{
		uint32_t temp = speed;										// Transfer requested speed
		if ((ioctl(spiHandle->spi_fd, SPI_IOC_WR_MAX_SPEED_HZ, &temp) >= 0) && // Set write speed
			(ioctl(spiHandle->spi_fd, SPI_IOC_RD_MAX_SPEED_HZ, &temp) >= 0))
		{
			spiHandle->spi_speed = speed;							// Hold the speed setting
			return true;											// Return success with speed change
		}
	}
	return false;													// Speed change failed
}

/*-[ SpiSetChipSelect ]-----------------------------------------------------}
. Given a valid SPI handle sets the SPI chip select mode to that given.
. RETURN: true for success, false for any failure
.--------------------------------------------------------------------------*/
bool SpiSetChipSelect (SPI_HANDLE spiHandle, SPIChipSelect CS_Mode)
{
	if (spiHandle && spiHandle->inuse)								// SPI handle valid and SPI handle is in use
	{
		switch (CS_Mode)
		{
			case SPI_CS_Mode_HIGH:									// CS HIGH FOR SELECT MODE
				spiHandle->mode |= SPI_CS_HIGH;
				spiHandle->mode &= ~SPI_NO_CS;
				break;
			case SPI_CS_Mode_LOW:									// CS LOW FOR SELECT MODE
				spiHandle->mode &= ~SPI_CS_HIGH;
				spiHandle->mode &= ~SPI_NO_CS;
				break;
			case SPI_CS_Mode_NONE:									// NO CS SELECT MODE
				spiHandle->mode |= SPI_NO_CS;
				break;
			default:
				return false;										// Invalid mode failure
		}
		if (ioctl(spiHandle->spi_fd , SPI_IOC_WR_MODE, &spiHandle->mode) >= 0) 
		{	
			return true;											// Return success
		}
	}
	return false;													// Return failure
}

/*-[ SpiSetBitOrder ]-----------------------------------------------------}
. Given a valid SPI handle sets the SPI bit order(LSB/MSB) to that given.
. RETURN: true for success, false for any failure
.--------------------------------------------------------------------------*/
bool SpiSetBitOrder (SPI_HANDLE spiHandle, SPIBitOrder Order)
{
	if (spiHandle && spiHandle->inuse)								// SPI handle valid and SPI handle is in use
	{
		switch (Order)
		{
			case SPI_BIT_ORDER_LSBFIRST:							// LSB MODE SELECTED
				spiHandle->mode |= SPI_LSB_FIRST;
				break;
			case SPI_BIT_ORDER_MSBFIRST:							// MSB MODE SELECTED
				spiHandle->mode &= ~SPI_LSB_FIRST;
				break;
			default:
				return false;										// Invalid mode failure
		}
		if (ioctl(spiHandle->spi_fd, SPI_IOC_WR_MODE, &spiHandle->mode) >= 0)
		{
			return true;											// Return success
		}
	}
	return false;													// Return failure
}

/*-[ SpiSetBitsPerWord ]----------------------------------------------------}
. Given a valid SPI handle sets the SPI bits per transmission word.
. RETURN: true for success, false for any failure
.--------------------------------------------------------------------------*/
bool SpiSetBitsPerWord(SPI_HANDLE spiHandle, uint8_t bits)
{
	if (spiHandle && spiHandle->inuse && bits > 0)					// SPI handle valid and SPI handle is in use
	{
		uint8_t spi_bitsPerWord = bits;								// Create a temp variable
		if ((ioctl(spiHandle->spi_fd, SPI_IOC_WR_BITS_PER_WORD, &spi_bitsPerWord) >= 0) &&
			(ioctl(spiHandle->spi_fd, SPI_IOC_RD_BITS_PER_WORD, &spi_bitsPerWord) >= 0))
		{
			spiHandle->spi_bitsPerWord = bits;						// Hold the bits per word
			return true;											// Return success
		}
	}
	return false;													// Return failure
}


/*-[ SpiWriteAndRead ]------------------------------------------------------}
. Given a valid SPI handle and valid data pointers the call will send and
. receive data to and from the buffer pointers. As the write occurs before
. the read the buffer pointers can be the same buffer space.
. RETURN: >= 0 transfer count for success, < 0 for any error  
.--------------------------------------------------------------------------*/
int SpiWriteAndRead (SPI_HANDLE spiHandle, uint8_t* TxData, uint8_t* RxData, uint16_t Length, bool LeaveCsLow)
{
	int retVal = -1;												// Preset -1
	if (spiHandle && spiHandle->inuse)								// SPI handle valid and SPI handle is in use
	{
		if (spiHandle->uselocks)									// Using locks
		{
			sem_wait(&spiHandle->lock);								// Take semaphore
		}
		struct spi_ioc_transfer spi = { 0 };
		spi.tx_buf = (unsigned long)TxData;							// transmit from "data"
		spi.rx_buf = (unsigned long)RxData;							// receive into "data"
		spi.len = Length;											// length of data to tx/rx
		spi.delay_usecs = 0;										// Delay before sending
		spi.speed_hz = spiHandle->spi_speed;						// Speed for transfer
		spi.bits_per_word = spiHandle->spi_bitsPerWord;				// Bits per exchange
		spi.cs_change = LeaveCsLow;									// 0=Set CS high after a transfer, 1=leave CS set low
		retVal = ioctl(spiHandle->spi_fd, SPI_IOC_MESSAGE(1), &spi);// Execute exchange
		if (spiHandle->uselocks)									// Using locks
		{
			sem_post(&spiHandle->lock);							    // Give semaphore
		}
	}
	return retVal;													// Return result
}

/*-[ SpiWriteBlockRepeat ]--------------------------------------------------}
. Given a valid SPI handle and valid data pointers the call will send the
. data block repeat times. It is used to speed up things like writing to SPI
. LCD screen with areas a fixed colour.
. RETURN: >= 0 blocks transfered for success, < 0 for any error  
.--------------------------------------------------------------------------*/
int SpiWriteBlockRepeat (SPI_HANDLE spiHandle, uint8_t* TxBlock, uint16_t TxBlockLen, uint32_t Repeats, bool LeaveCsLow)
{
	int retVal = -1;												// Preset -1
	if (spiHandle && TxBlock && spiHandle->inuse)					// SPI handle and TxBlock valid and SPI handle is in use
	{
		if (spiHandle->uselocks)									// Using locks
		{
			sem_wait(&spiHandle->lock);								// Take semaphore
		}
		struct spi_ioc_transfer spi = { 0 };
		spi.tx_buf = (unsigned long)TxBlock;						// transmit from "data"
		spi.rx_buf = (unsigned long)0;          					// receive into "data"
		spi.len = TxBlockLen;										// length of data to tx/rx
		spi.delay_usecs = 0;										// Delay before sending
		spi.speed_hz = spiHandle->spi_speed;						// Speed for transfer
		spi.bits_per_word = spiHandle->spi_bitsPerWord;				// Bits per exchange
		spi.cs_change = LeaveCsLow;									// 0=Set CS high after a transfer, 1=leave CS set low
        retVal = 0;                                                 // Zero retVal 
        uint32_t j;
        for (j = 0; j < Repeats && retVal == TxBlockLen; j++)       // For each block repeat
        {    
            retVal = ioctl(spiHandle->spi_fd, SPI_IOC_MESSAGE(1), &spi);// Execute exchange
        }
        retVal = j;                                                 // Return block count
		if (spiHandle->uselocks)									// Using locks
		{
			sem_post(&spiHandle->lock);							    // Give semaphore
		}
	}
	return retVal;													// Return result
}
