#ifndef GBK_CONV_H
#define GBK_CONV_H

#include <stdint.h>

unsigned short *gb_str_to_ucs2be(const unsigned char *gb_code, unsigned int *out_len);
char *ucs2be_str_to_gb(const unsigned short *uni_str, unsigned int *out_len);
char *utf8_str_to_gb(const unsigned char *utf8, unsigned int *out_len);
char *ucs2be_str_to_utf8(const unsigned char *unicode, unsigned int *out_len);
/** 客体内存常见为 UTF-16/UCS-2 小端字节序（与 ucs2be_str_to_utf8 的「高字节在前」相反）。 */
char *ucs2le_str_to_utf8(const unsigned char *unicode, unsigned int *out_len);

#endif
