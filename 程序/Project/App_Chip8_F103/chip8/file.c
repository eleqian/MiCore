#include "base.h"
#include "ff.h"
#include "file.h"
#include <stdio.h>
#include <string.h>

static struct {
    FIL fObj;               // file object
    uint16_t size;          // file size
    BOOL is_open;           // file is open
    char name[12];          // filename (8.3)
    char msg[64];           // message
} file;

/*-----------------------------------*/

BOOL file_is_open(void)
{
    return file.is_open;
}
uint16_t file_get_size(void)
{
    return file.size;
}
const char *file_get_name(void)
{
    return file.name;
}

/*-----------------------------------*/

void file_init(void)
{
    memset(&file, 0, sizeof(file));
}

BOOL file_open(const char *fn)
{
    FIL *fd = &file.fObj;
    size_t i;
    BOOL ret = TRUE;

    if (file.is_open) {
        f_close(fd);
        file.is_open = FALSE;
    }

    if ((fn != NULL) && (*fn != '\0')/* && (strlen(fn) < sizeof(file.name))*/) {
        /* get filename without path */
        i = strlen(fn);
        while (i--) {
            if ((fn[i] == '\\') || (fn[i] == '/')) {
                i++;
                break;
            }
        }
        strncpy(file.name, fn + i, sizeof(file.name));
        //strncpy(file.name, fn, sizeof(file.name));
        file.name[sizeof(file.name) - 1] = '\0';

        if (f_open(fd, fn, FA_OPEN_EXISTING | FA_READ) != FR_OK) {
            sprintf(file.msg, "File not found.");
            ret = FALSE;
        } else {
            file.size = f_size(fd);
            file.is_open = TRUE;
            sprintf(file.msg, "File open OK.");
            ret = TRUE;
        }
    } else {
        sprintf(file.msg, "Invalide file path.");
        ret = FALSE;
    }

    return ret;
}

uint16_t file_read(uint8_t *buf, uint16_t maxsize)
{
    uint16_t ret;
    UINT br;
    FIL *fd = &file.fObj;

    if (file.is_open) {
        f_lseek(fd, 0);
        if ((file.size > maxsize) || (file.size == 0)) {
            sprintf(file.msg, "Wrong filesize.(>%dB)", maxsize);
            ret = 0;
        } else if (f_read(fd, buf, file.size, &br) != FR_OK) {
            sprintf(file.msg, "Could not read file.");
            ret = 0;
        } else {
            sprintf(file.msg, "File read OK.");
            ret = br;
        }
    } else {
        sprintf(file.msg, "No open file.");
        ret = 0;
    }

    return ret;
}

BOOL file_close(void)
{
    FIL *fd = &file.fObj;
    BOOL ret;

    ret = file.is_open;
    f_close(fd);
    file.is_open = FALSE;

    return ret;
}
