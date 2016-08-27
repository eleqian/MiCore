/*
Here are the functions for Hamming code 7.4 and Extended Hamming code 8.4
encoding and decoding.
For 7.4 code, one error per 7-bit codeword can be corrected.
For 8.4 code, one error per 8-bit codeword can be corrected
and not less than 2 errors can be detected.

The generator polinomial used: X3+X1+1
Also, X3+X2+1 may be used.

(c)VLV
vlv@writeme.com
*/

#include "hamm7484.h"

#define INVALID_DATA        0xFF

unsigned char const encode_table[16] = {
    0x00, 0x0b, 0x16, 0x1d, 0x27, 0x2c, 0x31, 0x3a,
    0x45, 0x4e, 0x53, 0x58, 0x62, 0x69, 0x74, 0x7f
};

unsigned char const correction_table[8] = {
    0x00, 0x01, 0x02, 0x08, 0x04, 0x40, 0x10, 0x20
};

/*
Hamming 7.4 code encoding
input is 4-bit data
output is 7-bit Hamming codeword
*/
unsigned char Encode_74(unsigned char x)
{
    return encode_table[x];
}

/*
 Hamming 7.4 code correction and decoding
 input is 7-bit Hamming codeword
 output is 4-bit recovered data
 Corrects 1 error per codeword
*/
unsigned char Decode_74(unsigned char x)
{
    return (x ^ correction_table[x ^ encode_table[x >> 3]]) >> 3;
}

/*
Parity Extended Hamming 8.4 code encoding
input is 4-bit data
output is 8-bit codeword
*/
unsigned char Encode_84(unsigned char x)
{
    unsigned char c, d, p;

    c = d = encode_table[x];
    p = 0;

    // Calculate parity
    while (d) {
        p ^= d;
        d >>= 1;
    }

    return (c << 1) | (p & 1);
}

/*
Parity Extended Hamming 8.4 code correction and decoding
input is 8-bit codeword
output is 4-bit decoded data
Corrects 1 error per codeword, detects not less than 2 errors
*/
unsigned char Decode_84(unsigned char x)
{
    unsigned char p, s;

    // Calculate parity
    s = x;
    p = 0;
    while (s) {
        p ^= s;
        s >>= 1;
    }
    p &= 1;

    // Calculate syndrome
    s = (x >> 1) ^ encode_table[x >> 4];
    if (s) {
        if (!p) {
            return INVALID_DATA;    // more than 1 error - can't correct
        }
        return (x ^ (correction_table[s] << 1)) >> 4;
    }

    return x >> 4;
}
