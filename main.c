#define _DEFAULT_SOURCE
#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <pthread.h>
#include <linux/spi/spidev.h> // Needed for SPI_MODE_3

#include <time.h>
#include <semaphore.h> // need to share access to screen

#include "gpio.h"
#include "spi.h"
#include "ssd1327.h"


static sem_t lock;

static void* ticktask  (void* param)
{
	char buf[17];
	HDC Dc = GetDC();		  // Fetch DC for this task
	SelectFont(Dc, FONT8x8);  // Small font for time
	while (1)
	{
        time_t t = time(NULL);
		struct tm* tm = localtime(&t);
		sprintf(buf, "Time: %02u:%02u:%02u", tm->tm_hour, tm->tm_min, tm->tm_sec);
		sem_wait(&lock);
		SSD1327_WriteText(Dc, 0, 40, &buf[0]);
		sem_post(&lock);
		sleep(1);
	}
	ReleaseDC(Dc);
	return 0;
}

static void* counttask (void* param)
{
	char buf[17];
	HDC Dc = GetDC();	// Fetch DC for this task
	uint16_t i = 0;
	while (1)
	{
		sprintf(buf, "i=%05u", i);
		sem_wait(&lock);
		SSD1327_WriteText(Dc, 0, 72, &buf[0]);
		sem_post(&lock);
		usleep(111111); 
		i++;
	}
	ReleaseDC(Dc);
	return 0;
}



static GPIO_HANDLE gpio = 0;
static SPI_HANDLE spi = 0;

int main (void) 
{
	gpio = GPIO_Open(0x0, 0x1000);									// Open GPIO access
	if (gpio == 0)													// Check it opened
	{
		fprintf(stderr, "Error setting up GPIO\n");
		return 1;
	}

	GPIO_Setup(gpio, 25, GPIO_OUTPUT);								// GPIO25 to output mode .. reset pin for SSD1327
	GPIO_Output(gpio, 25, 1);										// Set to high
	GPIO_Setup(gpio, 24, GPIO_OUTPUT);								// GPIO24 to DATA/CMD mode for SSD1327
	GPIO_Output(gpio, 24, 1);										// Set to high

	spi = SpiOpenPort(0, 8, 10000000, SPI_MODE_3, false);			// Initialize SPI 0 for SSD1327 10Mhz, SPI_MODE_3 
	if (spi == NULL)												// Check SPI opened
	{
		fprintf(stderr, "SPI device could not open\n");
		return 1;
	}

	usleep(100000);													// sleep for 100mS (RESET high = 100ms)
	GPIO_Output(gpio, 25, 0);										// SSD1327 reset low
	usleep(100000);													// sleep for 100mS (RESET LOW = 100ms)
	GPIO_Output(gpio, 25, 1);										// SSD1327 reset back high
	usleep(100000);													// sleep for 100mS  (RESET HIGH = 100ms)
	if (SSD1327_Open(spi, gpio, 24) == false)						// Open the SSD1327 which sends initialize string
    {
		fprintf(stderr, "SSD1327 device could not open\n");
		return 1;
	}
	usleep(200000);													// sleep for 200mS  (After initialize cmds sent)
	SSD1327_ScreenOnOff(1);											// Set screen on

	SSD1327_ClearScreen(0);

	HDC Dc = GetDC();
	SelectFont(Dc, FONT6x8);
	SSD1327_WriteText(Dc, 0, 0, "HELLO WORLD IN 6x8");
	SSD1327_WriteText(Dc, 0, 128-8, "BOTTOM LINE IN 6x8");
        
	sem_init(&lock, 0, 1);

	pthread_t taskhandle[2];
	pthread_create(&taskhandle[0], NULL, ticktask, NULL);
	pthread_create(&taskhandle[1], NULL, counttask, NULL);

	getchar();														// Wait of a keypress

	pthread_cancel(taskhandle[0]);
	pthread_cancel(taskhandle[1]);
    pthread_join(taskhandle[0], NULL);
    pthread_join(taskhandle[1], NULL);

	sem_destroy(&lock);
    SpiClosePort(spi);
	return (0);														// Exit program wioth no error
}


