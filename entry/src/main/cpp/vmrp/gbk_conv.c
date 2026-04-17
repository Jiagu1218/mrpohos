#include "gbk_conv.h"
#include <stdlib.h>
#include <string.h>
#include "./mythroad/include/tables.h"

unsigned short *gb_str_to_ucs2be(const unsigned char *gb_code, unsigned int *out_len) {
    unsigned int i = 0, j = 0, len;
    unsigned short *unicode;

    if (!gb_code) return NULL;

    while (gb_code[i]) {
        j++;
        if (gb_code[i] <= 0x80) {
            i += 1;
        } else if (gb_code[i + 1] == '\0') {
            break;
        } else {
            i += 2;
        }
    }
    len = (j + 1) * sizeof(unsigned short);
    unicode = (unsigned short *)malloc(len);
    if (out_len) *out_len = len;
    if (!unicode) return NULL;
    i = j = 0;
    while (gb_code[i]) {
        if (gb_code[i] <= 0x7F) {
            unicode[j] = gb_code[i] << 8;
            i += 1;
        } else if (gb_code[i] == 0x80) {
            unicode[j] = 0xAC20;
            i += 1;
        } else {
            unicode[j] = 0xFDFF;
            if (gb_code[i + 1] != '\0') {
                unsigned short code = gb_code[i] << 8 | gb_code[i + 1];
                if ((code >= 0x8140) && (code <= 0xFE4F)) {
                    int First = 0;
                    int Last = TAB_GB2UCS_8140_FE4F_SIZE - 1;
                    while (Last >= First) {
                        int Mid = (First + Last) >> 1;
                        gb2ucs_st data = tab_gb2ucs_8140_FE4F[Mid];
                        if (code < data.gb) {
                            Last = Mid - 1;
                        } else if (code > data.gb) {
                            First = Mid + 1;
                        } else if (code == data.gb) {
                            unsigned short v = data.ucs;
                            unicode[j] = (v << 8) | (v >> 8);
                            break;
                        }
                    }
                }
            }
            i += 2;
        }
        j++;
    }
    unicode[j] = '\0';
    return unicode;
}

static unsigned short ucs2le_char_to_gb(unsigned short ucs) {
    if (ucs >= 0x4E00 && ucs <= 0x9FA5) {
        return ucs2gb_4e00_9fa5[ucs - 0x4E00];
    } else {
        int First = 0;
        int Last = UCS2GB_OTHER_SIZE - 1;
        while (Last >= First) {
            int Mid = (First + Last) >> 1;
            if (ucs < ucs2gb_other[Mid].ucs) {
                Last = Mid - 1;
            } else if (ucs > ucs2gb_other[Mid].ucs) {
                First = Mid + 1;
            } else if (ucs == ucs2gb_other[Mid].ucs) {
                return ucs2gb_other[Mid].gb;
            }
        }
    }
    return 0xA1F4;
}

char *ucs2be_str_to_gb(const unsigned short *uni_str, unsigned int *out_len) {
    unsigned int len = 1, i = 0;
    const unsigned short *p = uni_str;
    unsigned char *gb;

    while (*p) {
        unsigned short tmp = (*p << 8) | (*p >> 8);
        if (tmp < 0x80) {
            len += 1;
        } else {
            len += 2;
        }
        p++;
    }
    gb = (unsigned char *)malloc(len);
    if (out_len) *out_len = len;
    if (!gb) return NULL;

    p = uni_str;
    while (*p) {
        unsigned short tmp = (*p << 8) | (*p >> 8);
        if (tmp < 0x80) {
            gb[i++] = (unsigned char)tmp;
        } else {
            unsigned short Gb = ucs2le_char_to_gb(tmp);
            gb[i++] = (unsigned char)(Gb >> 8);
            gb[i++] = (unsigned char)(Gb & 0xff);
        }
        p++;
    }
    gb[i] = '\0';
    return (char *)gb;
}

char *utf8_str_to_gb(const unsigned char *utf8, unsigned int *out_len) {
    unsigned int len = 1;
    unsigned short c;
    const unsigned char *s = utf8;
    unsigned char *gb, *mem;

    while (*s) {
        if (*s < 0x80) {
            len += 1;
            s += 1;
            continue;
        } else if ((*s & 0xe0) == 0xc0) {
            s += 2;
        } else if ((*s & 0xf0) == 0xe0) {
            s += 3;
        } else {
            break;
        }
        len += 2;
    }
    mem = (unsigned char *)malloc(len);
    if (out_len) *out_len = len;
    gb = mem;
    if (!gb) return NULL;
    s = utf8;
    while (*s) {
        if (*s < 0x80) {
            *gb++ = *s++;
            continue;
        } else if ((*s & 0xe0) == 0xc0) {
            c = ucs2le_char_to_gb(((s[0] & 0x1f) << 6) | (s[1] & 0x3f));
            s += 2;
        } else if ((*s & 0xf0) == 0xe0) {
            c = ucs2le_char_to_gb(((s[0] & 0x0f) << 12) | ((s[1] & 0x3f) << 6) | (s[2] & 0x3f));
            s += 3;
        } else {
            break;
        }
        *gb++ = (unsigned char)(c >> 8);
        *gb++ = (unsigned char)(c & 0xff);
    }
    *gb = '\0';
    return (char *)mem;
}

char *ucs2be_str_to_utf8(const unsigned char *unicode, unsigned int *out_len) {
    int u = 0, i = 0, len = 1;
    char *utf8;
    while ((unicode[u] || unicode[u + 1])) {
        if (unicode[u] == 0 && unicode[u + 1] < 0x80) {
            len += 1;
        } else if ((unicode[u] & 0xf8) == 0) {
            len += 2;
        } else {
            len += 3;
        }
        u += 2;
    }
    utf8 = (char *)malloc(len);
    if (out_len) *out_len = len;
    if (!utf8) return NULL;
    u = 0;
    while ((unicode[u] || unicode[u + 1])) {
        if (unicode[u] == 0 && unicode[u + 1] < 0x80) {
            utf8[i++] = unicode[u + 1];
        } else if ((unicode[u] & 0xf8) == 0) {
            utf8[i++] = 0xc0 | (unicode[u] << 2) | (unicode[u + 1] >> 6);
            utf8[i++] = 0x80 | (unicode[u + 1] & 0x3f);
        } else {
            utf8[i++] = 0xe0 | (unicode[u] >> 4);
            utf8[i++] = 0x80 | ((unicode[u] & 0x0f) << 2) | (unicode[u + 1] >> 6);
            utf8[i++] = 0x80 | (unicode[u + 1] & 0x3f);
        }
        u += 2;
    }
    utf8[i] = '\0';
    return utf8;
}

char *ucs2le_str_to_utf8(const unsigned char *unicode, unsigned int *out_len) {
    unsigned int i;
    unsigned char *swapped;
    char *ret;

    if (unicode == NULL) {
        char *r = (char *)malloc(1);
        if (!r) {
            return NULL;
        }
        r[0] = '\0';
        if (out_len) {
            *out_len = 1;
        }
        return r;
    }
    if (!unicode[0] && !unicode[1]) {
        return ucs2be_str_to_utf8(unicode, out_len);
    }

    i = 0;
    while (unicode[i] || unicode[i + 1]) {
        i += 2;
    }

    swapped = (unsigned char *)malloc(i + 2);
    if (!swapped) {
        return NULL;
    }
    for (unsigned int u = 0; u < i; u += 2) {
        swapped[u] = unicode[u + 1];
        swapped[u + 1] = unicode[u];
    }
    swapped[i] = 0;
    swapped[i + 1] = 0;

    ret = ucs2be_str_to_utf8(swapped, out_len);
    free(swapped);
    return ret;
}
