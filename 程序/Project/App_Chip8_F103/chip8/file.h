#ifndef FILE_H
#define FILE_H

#include "base.h"

void file_init(void);
BOOL file_open(const char *fn);
BOOL file_close(void);
uint16_t file_read(uint8_t *buf, uint16_t maxsize);

BOOL file_is_open(void);
uint16_t file_get_size(void);
const char *file_get_name(void);

#endif /* FILE_H */
