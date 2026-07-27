#include <stdint.h>
#include <string.h>
#include "main.h"
#include "stm32f4xx.h"
#include "stm32f4xx_hal_def.h"
#include "stm32f4xx_hal_gpio.h"
#include "stm32f4xx_hal_spi.h"
#include "diskio.h"

#define CMD_0 0x00
#define CMD_0_CRC 0x95
#define STUFF_BITS 0x0
#define CMD_8 0x08
#define CMD_8_ARG 0x000001AA
#define CMD_8_CRC 0x87
#define CMD_55 0x37
#define ACMD41_CMD 0x29
#define ACMD41_ARG 0x40000000
#define CMD_58 0x3A
#define MAX_RETRIES 20

uint8_t tx = 0xFF;


DSTATUS SD_WaitResponse(SPI_HandleTypeDef *hspi, uint8_t *response, uint16_t max_retries) {

    uint8_t rx = 0xFF;

    for (int i = 0; i < max_retries; i++) {
        // Send dummy byte to SD Card
        HAL_SPI_TransmitReceive(hspi, &tx, &rx, 1, 100);
        __NOP();

        // Check if the 7th bit in response is 0
        if ((rx & 0x80) == 0) {
            *response = rx;
            return SUCCESS;
        }
    }

    return STA_NOINIT;
}

void SD_SendCommand(SPI_HandleTypeDef *hspi, uint8_t cmd, uint32_t arg, uint8_t crc) {

    // CS toggle between commands
    HAL_GPIO_WritePin(CS_GPIO_Port, CS_Pin, 1);
    HAL_SPI_Transmit(hspi, &tx, 1, 100);
    HAL_GPIO_WritePin(CS_GPIO_Port, CS_Pin, 0);
    HAL_SPI_Transmit(hspi, &tx, 1, 100);

    // Must be sent in BE, STM32 sends in LE. Put MSB at the front of cmd frame
    uint8_t cmd_frame[6];

    cmd_frame[0] = 0x40 | cmd;
    cmd_frame[1] = arg >> 24;
    cmd_frame[2] = arg >> 16;
    cmd_frame[3] = arg >> 8;
    cmd_frame[4] = arg;
    cmd_frame[5] = crc;

    HAL_SPI_Transmit(hspi, cmd_frame, 6, 100);
}

DSTATUS SD_Init(SPI_HandleTypeDef *hspi) {

    uint8_t rx;
    DSTATUS status;

    // Power up
    HAL_GPIO_WritePin(CS_GPIO_Port, CS_Pin, 1);

    uint8_t dummy_bytes[10];
    memset(dummy_bytes, 0xFF, sizeof(dummy_bytes));
    HAL_SPI_Transmit(hspi, dummy_bytes, 10, 100);


    // CS Asserted
    HAL_GPIO_WritePin(CS_GPIO_Port, CS_Pin, 0);

    SD_SendCommand(hspi, CMD_0, STUFF_BITS, CMD_0_CRC);
    status = SD_WaitResponse(hspi, &rx, MAX_RETRIES);

    // Check if in idle state + no errors for CMD8
    if (rx != 0x01) {
        status = STA_NOINIT;
        return status;
    } else {
        rx = 0x00;
    }

    // Send CMD8
    uint8_t r7[4];

    SD_SendCommand(hspi, CMD_8, CMD_8_ARG, CMD_8_CRC);
    status = SD_WaitResponse(hspi, &rx, MAX_RETRIES);

    // Check for idle status + no errors

    if (rx != 0x01) {
        status = STA_NOINIT;
        return status;
    } else {

        // Shift out R7
        rx = 0x00;
        for (int i = 0; i < 4; i++) {
            HAL_SPI_TransmitReceive(hspi, &tx, &r7[i], 1, 100);
        }
    }

    uint8_t voltage = r7[2];
    uint8_t check_pattern = r7[3];

    // Check if voltage and check pattern is echo'd back exactly
    if (voltage != 0x01 || check_pattern != 0xAA) {
        status = STA_NOINIT;
        return status;
    }


    // --- ACMD41 LOOP ---

    for (int i = 0; i < MAX_RETRIES; i++) {
         // Send CMD55 to enable application commands
        SD_SendCommand(hspi, CMD_55, STUFF_BITS, 0x01);
        status = SD_WaitResponse(hspi, &rx, MAX_RETRIES);

        // Check for errors
        if (rx != 0x01) {
            status = STA_NOINIT;
            return status;
        }

        // Send ACMD41 until rx is set to 0x00. 
        SD_SendCommand(hspi, ACMD41_CMD, ACMD41_ARG, 0x01);
        status = SD_WaitResponse(hspi, &rx, MAX_RETRIES);

        // Check if rx is 0x00
        if (rx == 0x00) {
            // Card is initialized
            break;
        }
    }

    if (rx != 0x00) {
        status = STA_NOINIT;
        return status;
    }

    // Send CMD58

    uint8_t r3[4];

    SD_SendCommand(hspi, CMD_58, STUFF_BITS, 0x01);
    status = SD_WaitResponse(hspi, &rx, MAX_RETRIES);

    // Check for errors, 0x00 because card no longer idle, its been initialized.
    if (rx != 0x00) {
        status = STA_NOINIT;
        return status;
    } else {
        // Push out OCR
        for (int i = 0; i < 4; i++) {
            HAL_SPI_TransmitReceive(hspi, &tx, &r3[i], 1, 100);
        }
    }

    // Check CCS bit
    uint8_t CCS_bit = r3[0] & 0x40;

    if (CCS_bit == 0x40) {
        // High capacity, use block addressing 
    } else {
        // Standard, use byte addressing
    }

    return status;
}