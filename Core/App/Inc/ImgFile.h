/**
  ******************************************************************************
  * @file           : ImgFile.h
  * @brief          : Shared image file format stored in W25Q64
  ******************************************************************************
  */
#ifndef __IMG_FILE_H
#define __IMG_FILE_H

#include "oled.h"

/* Drawing region, see README W25Q64 partition table */
#define DRAW_SECTOR_BASE   288U
#define DRAW_SECTOR_COUNT  256U       //

#define DRAW_MAGIC0  'D'
#define DRAW_MAGIC1  'R'
#define DRAW_MAGIC2  'W'
#define DRAW_MAGIC3  '1'

//前4个字节为图片标志，接下来的12个字节为图片名称，剩余的部分为图片数据，图片数据大小为OLED_BUFFER_SIZE
typedef struct {
    uint8_t magic[4];
    char name[12];
    uint8_t data[OLED_BUFFER_SIZE];
} DrawFile_t;

#endif /* __IMG_FILE_H */
