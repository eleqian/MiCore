/*-----------------------------------------------------------------------*/
/* Low level disk I/O module skeleton for FatFs     (C)ChaN, 2013        */
/*-----------------------------------------------------------------------*/
/* If a working storage control module is available, it should be        */
/* attached to the FatFs via a glue function rather than modifying it.   */
/* This is an example of glue functions to attach various exsisting      */
/* storage control module to the FatFs module with a defined API.        */
/*-----------------------------------------------------------------------*/

#include "base.h"
#include "diskio.h"     /* FatFs lower layer API */
#include "spi_flash.h"  /* SPI Flash contorl */

/* Definitions of physical drive number for each media */
#define SPI_FLASH     0

/*-----------------------------------------------------------------------*/
/* Inidialize a Drive                                                    */
/*-----------------------------------------------------------------------*/
/* Physical drive nmuber (0..) */
DSTATUS disk_initialize(BYTE pdrv)
{
    DSTATUS stat = 0;

    switch (pdrv) {
    case SPI_FLASH:
        SPI_Flash_Init();
        return stat;
    default:
        break;
    }

    return STA_NOINIT;
}

/*-----------------------------------------------------------------------*/
/* Get Disk Status                                                       */
/*-----------------------------------------------------------------------*/
/* Physical drive nmuber (0..) */
DSTATUS disk_status(BYTE pdrv)
{
    DSTATUS stat = 0;

    switch (pdrv) {
    case SPI_FLASH:
        return stat;
    default:
        break;
    }

    return STA_NOINIT;
}

/*-----------------------------------------------------------------------*/
/* Read Sector(s)                                                        */
/*-----------------------------------------------------------------------*/
/* Physical drive nmuber (0..) */
/* Data buffer to store read data */
/* Sector address (LBA) */
/* Number of sectors to read (1..128) */
DRESULT disk_read(BYTE pdrv, BYTE *buff, DWORD sector, UINT count)
{
    DRESULT res = RES_PARERR;

    switch (pdrv) {
    case SPI_FLASH:
        SPI_Flash_Read(sector << 12, buff, count << 12);
        res = RES_OK;
        break;
    default:
        break;
    }

    return res;
}

/*-----------------------------------------------------------------------*/
/* Write Sector(s)                                                       */
/*-----------------------------------------------------------------------*/
/* Physical drive nmuber (0..) */
/* Data to be written */
/* Sector address (LBA) */
/* Number of sectors to write (1..128) */
#if _USE_WRITE
DRESULT disk_write(BYTE pdrv, const BYTE *buff, DWORD sector, UINT count)
{
    DRESULT res = RES_PARERR;
    UINT i;

    switch (pdrv) {
    case SPI_FLASH:
        for (i = 0; i < count; i++) {
            SPI_Flash_SectorErase((sector + i) << 12);
        }
        SPI_Flash_Write(sector << 12, buff, count << 12);
        res = RES_OK;
        break;
    default:
        break;
    }

    return res;
}
#endif //_USE_WRITE

/*-----------------------------------------------------------------------*/
/* Miscellaneous Functions                                               */
/*-----------------------------------------------------------------------*/
/* Physical drive nmuber (0..) */
/* Control code */
/* Buffer to send/receive control data */
#if _USE_IOCTL
DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void *buff)
{
    DRESULT res = RES_PARERR;

    switch (pdrv) {
    case SPI_FLASH:
        switch (cmd) {
        case CTRL_SYNC:
            res = RES_OK;
            break;
        case GET_BLOCK_SIZE:
            *(DWORD *)buff = SPI_FLASH_SECTOR_SIZE;
            res = RES_OK;
            break;
        case GET_SECTOR_COUNT:
            *(DWORD *)buff = SPI_FLASH_SECTOR_COUNT;
            res = RES_OK;
            break;
        case GET_SECTOR_SIZE:
            *(WORD *)buff = SPI_FLASH_SECTOR_SIZE;
            res = RES_OK;
            break;
        default:
            break;
        }
        break;
    default:
        break;
    }

    return res;
}
#endif //_USE_IOCTL
