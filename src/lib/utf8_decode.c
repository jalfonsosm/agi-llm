/* utf8_decode.c - Björn Höhrmann's DFA UTF-8 decoder
 * Copyright (c) 2008-2009 Bjoern Hoehrmann <bjoern@hoehrmann.de>
 * See http://bjoern.hoehrmann.de/utf-8/decoder/dfa/ for details.
 */

#include "utf8_decode.h"

static const uint8_t utf8d[] = {
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,
  7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
  8,8,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
  10,3,3,3,3,3,3,3,3,3,3,3,3,4,3,3,11,6,6,6,5,8,8,8,8,8,8,8,8,8,8,8,

  0,12,24,36,60,96,84,12,12,12,48,72,12,12,12,12,12,12,12,12,12,12,12,12,
  12, 0,12,12,12,12,12, 0,12, 0,12,12,12,24,12,12,12,12,12,24,12,24,12,12,
  12,12,12,12,12,12,12,24,12,12,12,12,12,24,12,12,12,12,12,12,12,24,12,12,
  12,12,12,12,12,12,12,36,12,36,12,12,12,36,12,12,12,12,12,36,12,36,12,12,
  12,36,12,12,12,12,12,12,12,12,12,12,
};

uint32_t utf8_decode(uint32_t* state, uint32_t* codep, uint32_t byte) {
  uint32_t type = utf8d[byte];

  *codep = (*state != UTF8_ACCEPT) ?
    (byte & 0x3fu) | (*codep << 6) :
    (0xff >> type) & (byte);

  *state = utf8d[256 + *state + type];
  return *state;
}

/* Legacy API for compatibility */
static int  the_index = 0;
static int  the_length = 0;
static int  the_char = 0;
static int  the_byte = 0;
static char* the_input;

static int get() {
    if (the_index >= the_length) return UTF8_END;
    int c = the_input[the_index] & 0xFF;
    the_index += 1;
    return c;
}

static int cont() {
    int c = get();
    return ((c & 0xC0) == 0x80) ? (c & 0x3F) : UTF8_ERROR;
}

void utf8_decode_init(char p[], int length) {
    the_index = 0;
    the_input = p;
    the_length = length;
    the_char = 0;
    the_byte = 0;
}

int utf8_decode_at_byte(void) {
    return the_byte;
}

int utf8_decode_at_character(void) {
    return (the_char > 0) ? the_char - 1 : 0;
}

int utf8_decode_next(void) {
    int c, c1, c2, c3, r;
    if (the_index >= the_length) return the_index == the_length ? UTF8_END : UTF8_ERROR;
    the_byte = the_index;
    the_char += 1;
    c = get();
    if ((c & 0x80) == 0) return c;
    if ((c & 0xE0) == 0xC0) {
        c1 = cont();
        if (c1 >= 0) {
            r = ((c & 0x1F) << 6) | c1;
            if (r >= 128) return r;
        }
    } else if ((c & 0xF0) == 0xE0) {
        c1 = cont();
        c2 = cont();
        if ((c1 | c2) >= 0) {
            r = ((c & 0x0F) << 12) | (c1 << 6) | c2;
            if (r >= 2048 && (r < 55296 || r > 57343)) return r;
        }
    } else if ((c & 0xF8) == 0xF0) {
        c1 = cont();
        c2 = cont();
        c3 = cont();
        if ((c1 | c2 | c3) >= 0) {
            r = ((c & 0x07) << 18) | (c1 << 12) | (c2 << 6) | c3;
            if (r >= 65536 && r <= 1114111) return r;
        }
    }
    return UTF8_ERROR;
}
