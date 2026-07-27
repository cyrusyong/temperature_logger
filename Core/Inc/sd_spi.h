// Initialize SD Card

// Polling

// Send command

#include "stm32f4xx_hal_spi.h"
#include <stdint.h>

/**
 * @brief Sends dummy bytes until either response has 0 in 7th bit or max retries is hit
 * 
 * @param hspi SPI Handler
 * @param response Address pointing to where to store response
 * @param max_retries Number of tries before stopping
 * @return DSTATUS Status of the drive
 */
DSTATUS SD_WaitResponse(SPI_HandleTypeDef *hspi, uint8_t *response, uint16_t max_retries);

/**
 * @brief Sends command to SD card
 * 
 * @param hspi SPI Handler
 * @param cmd Command number
 * @param arg Arguments command needs
 * @param crc Check pattern
 */
void SD_SendCommand(SPI_HandleTypeDef *hspi, uint8_t cmd, uint32_t arg, uint8_t crc);

/**
 * @brief Initializes the SD card
 * 
 * @param hspi SPI Handler
 * @return DSTATUS Status of the drive
 */
DSTATUS SD_Init(SPI_HandleTypeDef *hspi);

