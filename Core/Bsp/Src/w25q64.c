/**
  ******************************************************************************
  * @file           : w25q64.c
  * @brief          : W25Q64 SPI NOR Flash driver over SPI1
  ******************************************************************************
  */

#include "w25q64.h"

extern SPI_HandleTypeDef hspi1;

/* W25Q64 command set */
#define W25Q64_CMD_READ_STATUS   0x05U
#define W25Q64_CMD_WRITE_ENABLE  0x06U
#define W25Q64_CMD_READ_DATA     0x03U
#define W25Q64_CMD_PAGE_PROGRAM  0x02U
#define W25Q64_CMD_SECTOR_ERASE  0x20U
#define W25Q64_CMD_CHIP_ERASE    0xC7U
#define W25Q64_CMD_JEDEC_ID      0x9FU

#define W25Q64_STATUS_WIP        0x01U

#define BSP_W25Q64_SPI_TIMEOUT       1000U
#define BSP_W25Q64_WAIT_TIMEOUT      1000U
#define BSP_W25Q64_ERASE_TIMEOUT     5000U
#define BSP_W25Q64_CHIP_ERASE_TIMEOUT 120000U

static void w25q64_cs_low(void)
{
    HAL_GPIO_WritePin(SPI1_NSS_GPIO_Port, SPI1_NSS_Pin, GPIO_PIN_RESET);
}

static void w25q64_cs_high(void)
{
    HAL_GPIO_WritePin(SPI1_NSS_GPIO_Port, SPI1_NSS_Pin, GPIO_PIN_SET);
}

static BSP_W25Q64_Status_t w25q64_transmit(const uint8_t *tx, uint32_t len)
{
    while (len > 0U)
    {
        uint16_t n = (len > 0xFFFFU) ? 0xFFFFU : (uint16_t)len;
        if (HAL_SPI_Transmit(&hspi1, (uint8_t *)tx, n, BSP_W25Q64_SPI_TIMEOUT) != HAL_OK)
        {
            return BSP_W25Q64_ERROR;
        }
        tx += n;
        len -= n;
    }
    return BSP_W25Q64_OK;
}

static BSP_W25Q64_Status_t w25q64_receive(uint8_t *rx, uint32_t len)
{
    while (len > 0U)
    {
        uint16_t n = (len > 0xFFFFU) ? 0xFFFFU : (uint16_t)len;
        if (HAL_SPI_Receive(&hspi1, rx, n, BSP_W25Q64_SPI_TIMEOUT) != HAL_OK)
        {
            return BSP_W25Q64_ERROR;
        }
        rx += n;
        len -= n;
    }
    return BSP_W25Q64_OK;
}

static BSP_W25Q64_Status_t w25q64_write_enable(void)
{
    BSP_W25Q64_Status_t st;
    uint8_t cmd = W25Q64_CMD_WRITE_ENABLE;

    w25q64_cs_low();
    st = w25q64_transmit(&cmd, 1U);
    w25q64_cs_high();
    return st;
}

static BSP_W25Q64_Status_t w25q64_wait_ready(uint32_t timeout_ms)
{
    uint32_t start = HAL_GetTick();
    uint8_t cmd = W25Q64_CMD_READ_STATUS;
    uint8_t status = 0x00U;

    for (;;)
    {
        BSP_W25Q64_Status_t st;

        w25q64_cs_low();
        st = w25q64_transmit(&cmd, 1U);
        if (st == BSP_W25Q64_OK) st = w25q64_receive(&status, 1U);
        w25q64_cs_high();

        if (st != BSP_W25Q64_OK) return st;
        if ((status & W25Q64_STATUS_WIP) == 0U) return BSP_W25Q64_OK;
        if ((HAL_GetTick() - start) >= timeout_ms) return BSP_W25Q64_ERR_TIMEOUT;
    }
}

BSP_W25Q64_Status_t BSP_W25Q64_Init(void)
{
    BSP_W25Q64_ID_t id;

    if (BSP_W25Q64_ReadID(&id) != BSP_W25Q64_OK)
    {
        return BSP_W25Q64_ERROR;
    }
    return (id.mfg == BSP_W25Q64_MFG_WINBOND) ? BSP_W25Q64_OK : BSP_W25Q64_ERR_ID;
}

BSP_W25Q64_Status_t BSP_W25Q64_ReadID(BSP_W25Q64_ID_t *p_id)
{
    BSP_W25Q64_Status_t st;
    uint8_t cmd = W25Q64_CMD_JEDEC_ID;

    if (p_id == NULL) return BSP_W25Q64_ERROR;

    w25q64_cs_low();
    st = w25q64_transmit(&cmd, 1U);
    if (st == BSP_W25Q64_OK) st = w25q64_receive(&p_id->mfg, 1U);
    if (st == BSP_W25Q64_OK) st = w25q64_receive(&p_id->mem_type, 1U);
    if (st == BSP_W25Q64_OK) st = w25q64_receive(&p_id->capacity, 1U);
    w25q64_cs_high();
    return st;
}

BSP_W25Q64_Status_t BSP_W25Q64_Read(uint32_t addr, uint8_t *p_data, uint32_t len)
{
    BSP_W25Q64_Status_t st;
    uint8_t cmd[4];

    if (p_data == NULL || len == 0U) return BSP_W25Q64_ERROR;
    if ((addr + len) > BSP_W25Q64_CAPACITY) return BSP_W25Q64_ERR_ADDR;

    cmd[0] = W25Q64_CMD_READ_DATA;
    cmd[1] = (uint8_t)(addr >> 16);
    cmd[2] = (uint8_t)(addr >> 8);
    cmd[3] = (uint8_t)addr;

    w25q64_cs_low();
    st = w25q64_transmit(cmd, 4U);
    if (st == BSP_W25Q64_OK) st = w25q64_receive(p_data, len);
    w25q64_cs_high();
    return st;
}

BSP_W25Q64_Status_t BSP_W25Q64_Write(uint32_t addr, const uint8_t *p_data, uint32_t len)
{
    BSP_W25Q64_Status_t st;
    uint8_t cmd[4];
    uint32_t offset = 0U;

    if (p_data == NULL || len == 0U) return BSP_W25Q64_ERROR;
    if ((addr + len) > BSP_W25Q64_CAPACITY) return BSP_W25Q64_ERR_ADDR;

    while (offset < len)
    {
        uint32_t page_avail = BSP_W25Q64_PAGE_SIZE - (addr & (BSP_W25Q64_PAGE_SIZE - 1U));
        uint32_t chunk = len - offset;
        uint32_t cur_addr = addr + offset;

        if (chunk > page_avail) chunk = page_avail;

        st = w25q64_wait_ready(BSP_W25Q64_WAIT_TIMEOUT);
        if (st != BSP_W25Q64_OK) return st;

        st = w25q64_write_enable();
        if (st != BSP_W25Q64_OK) return st;

        cmd[0] = W25Q64_CMD_PAGE_PROGRAM;
        cmd[1] = (uint8_t)(cur_addr >> 16);
        cmd[2] = (uint8_t)(cur_addr >> 8);
        cmd[3] = (uint8_t)cur_addr;

        w25q64_cs_low();
        st = w25q64_transmit(cmd, 4U);
        if (st == BSP_W25Q64_OK) st = w25q64_transmit(p_data + offset, chunk);
        w25q64_cs_high();
        if (st != BSP_W25Q64_OK) return st;

        offset += chunk;
    }

    return w25q64_wait_ready(BSP_W25Q64_WAIT_TIMEOUT);
}

BSP_W25Q64_Status_t BSP_W25Q64_EraseSector(uint32_t addr)
{
    BSP_W25Q64_Status_t st;
    uint8_t cmd[4];

    if ((addr % BSP_W25Q64_SECTOR_SIZE) != 0U) return BSP_W25Q64_ERR_ADDR;
    if (addr >= BSP_W25Q64_CAPACITY) return BSP_W25Q64_ERR_ADDR;

    st = w25q64_wait_ready(BSP_W25Q64_ERASE_TIMEOUT);
    if (st != BSP_W25Q64_OK) return st;

    st = w25q64_write_enable();
    if (st != BSP_W25Q64_OK) return st;

    cmd[0] = W25Q64_CMD_SECTOR_ERASE;
    cmd[1] = (uint8_t)(addr >> 16);
    cmd[2] = (uint8_t)(addr >> 8);
    cmd[3] = (uint8_t)addr;

    w25q64_cs_low();
    st = w25q64_transmit(cmd, 4U);
    w25q64_cs_high();
    if (st != BSP_W25Q64_OK) return st;

    return w25q64_wait_ready(BSP_W25Q64_ERASE_TIMEOUT);
}

BSP_W25Q64_Status_t BSP_W25Q64_EraseChip(void)
{
    BSP_W25Q64_Status_t st;
    uint8_t cmd = W25Q64_CMD_CHIP_ERASE;

    st = w25q64_wait_ready(BSP_W25Q64_ERASE_TIMEOUT);
    if (st != BSP_W25Q64_OK) return st;

    st = w25q64_write_enable();
    if (st != BSP_W25Q64_OK) return st;

    w25q64_cs_low();
    st = w25q64_transmit(&cmd, 1U);
    w25q64_cs_high();
    if (st != BSP_W25Q64_OK) return st;

    return w25q64_wait_ready(BSP_W25Q64_CHIP_ERASE_TIMEOUT);
}
