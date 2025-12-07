/* utf8_decode.h - Björn Höhrmann's UTF-8 decoder */
#ifndef UTF8_DECODE_H
#define UTF8_DECODE_H

#include <stdint.h>

#define UTF8_ACCEPT 0
#define UTF8_REJECT 12

/* Legacy defines for compatibility */
#define UTF8_END   -1
#define UTF8_ERROR -2

/* DFA decoder */
uint32_t utf8_decode(uint32_t* state, uint32_t* codep, uint32_t byte);

/* Legacy API for compatibility */
extern int  utf8_decode_at_byte(void);
extern int  utf8_decode_at_character(void);
extern void utf8_decode_init(char p[], int length);
extern int  utf8_decode_next(void);

#endif

