#ifndef __ECC1608_H__
#define __ECC1608_H__

unsigned short ecc1608_encode(unsigned char data);

unsigned char ecc1608_getsyn(unsigned short code);

unsigned char ecc1608_decode(unsigned char *data, unsigned short code);

#endif //__ECC1608_H__
