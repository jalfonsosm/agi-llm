/*
_sprintf                         cseg     00002374 0000001C
_printf                          cseg     00002390 00000073
_FormatDecimal                   cseg     00002403 0000001F
_FormatUnsigned                  cseg     00002422 00000011
_FormatHex                       cseg     00002433 00000011
_FormatString                    cseg     00002444 00000009
_FormatStringAX                  cseg     0000244D 00000011
_FormatChar                      cseg     0000245E 00000014
*/

#include <stdarg.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>

#include "../agi.h"

#include "printf.h"
#include "string.h"

// window_put_char
#include "../ui/window.h"
#include "../sys/chargen.h"
#include "../lib/utf8_decode.h"

static void format_string_ax(const char *str);
static void format_char(char ch);
static void format_utf8_string(const char *str);

/* UTF-8 decoder state for format_char */
static uint32_t format_char_utf8_state = 0;
static uint32_t format_char_utf8_codepoint = 0;

// void *format_ip = 0;
static char *format_strbuff = 0;
static int format_to_string = 0;	// boolean value

/*
?? sprintf()
{
	asdfkljasdlfk
}

*/

static char *di;	// sprintf string

#if 0
static int charCount(char *str)
{
	int count;

	assert(str);

	count = 0;
	str = strchr(str, '%');

	while (str != 0)
	{
		count++;
		str++;
		str = strchr(str, '%');
	}

	return count;
}
#endif

void agi_printf(const char *var8, ...)
{
	va_list ap;
	
	const char *si;
	s16 bx;
	char al;
	
	va_start(ap, var8);
	
	si = var8;
	di = format_strbuff;
	//bx = bp + 0xA;

	al = *(si++);

	while (al != 0)
	{
		if ( al != '%')
			format_char(al);
		else
		{

			switch (*(si++))
			{
				case 's':		// string
					format_string_ax(va_arg(ap, const char *));
					//bx += 2;
					break;
				
				case 'd':		// decimal
					bx = (s16)va_arg(ap, int);
					if (bx < 0)
					{
						format_char('-');
						format_string_ax(  int_to_string(bx * -1)  );
					}
					else
						format_string_ax(  int_to_string(bx)  );
					//bx += 2;
					break;
				
				case 'u':		// unsigned decimal
					format_string_ax(  int_to_string( (u16)va_arg(ap, int))  );
					//bx += 2;
					break;
				
				case 'x':		// hex number
					format_string_ax(  int_to_hex_string( (u16)va_arg (ap, int))  );
					//bx += 2;
					break;
				
				case 'c':		// character
					//al = *bx;
					//bx += 2;	// everything is pushed on as a word
					format_char((char)va_arg(ap, int));
					break;
				
				default:		// not recognised
					format_char('%');
					si--;
			}
		}
		al = *(si++);
	}
	

	if ( (format_to_string & 0xFF) != 0)
		*(di++) = 0;
	va_end (ap);
	ch_update();
}


static void format_string_ax(const char *str)
{
	/* Check if string contains UTF-8 multibyte sequences */
	const unsigned char *p = (const unsigned char *)str;
	int has_utf8 = 0;
	while (*p) {
		if (*p > 127) {
			has_utf8 = 1;
			break;
		}
		p++;
	}
	
	if (has_utf8) {
		printf("format_string_ax: UTF-8 detected, calling format_utf8_string\n");
		format_utf8_string(str);
	} else {
		/* ASCII path - original code */
		char al = *(str++);
		while (al != 0) {
			format_char(al);
			al = *(str++);
		}
	}
}

static void format_utf8_string(const char *str)
{
	const unsigned char *s = (const unsigned char *)str;
	uint32_t codepoint = 0;
	uint32_t state = UTF8_ACCEPT;
	
	while (*s) {
		uint32_t byte = (uint32_t)(*s++);
		uint32_t prev_state = state;
		
		utf8_decode(&state, &codepoint, byte);
		
		if (state == UTF8_ACCEPT) {
			/* Complete codepoint decoded */
			if (format_to_string != 0) {
				/* For string mode, encode back to UTF-8 */
				if (codepoint < 0x80) {
					*(di++) = (char)codepoint;
				} else if (codepoint < 0x800) {
					*(di++) = (char)(0xC0 | (codepoint >> 6));
					*(di++) = (char)(0x80 | (codepoint & 0x3F));
				} else if (codepoint < 0x10000) {
					*(di++) = (char)(0xE0 | (codepoint >> 12));
					*(di++) = (char)(0x80 | ((codepoint >> 6) & 0x3F));
					*(di++) = (char)(0x80 | (codepoint & 0x3F));
				} else {
					*(di++) = (char)(0xF0 | (codepoint >> 18));
					*(di++) = (char)(0x80 | ((codepoint >> 12) & 0x3F));
					*(di++) = (char)(0x80 | ((codepoint >> 6) & 0x3F));
					*(di++) = (char)(0x80 | (codepoint & 0x3F));
				}
			} else {
				/* Display mode - pass Unicode codepoint */
				if (codepoint > 127) {
					printf("format_utf8_string: decoded U+%04X\n", codepoint);
				}
				window_put_char(codepoint);
			}
			codepoint = 0;
		} else if (state == UTF8_REJECT) {
			/* Error - reset and skip */
			state = UTF8_ACCEPT;
			codepoint = 0;
			if (prev_state == UTF8_ACCEPT) {
				/* First byte was invalid, output it as-is */
				if (format_to_string != 0) {
					*(di++) = (char)byte;
				} else {
					window_put_char(byte);
				}
			}
		}
	}
}

static void format_char(char ch)
{
	if (format_to_string != 0) {
		*(di++) = ch;
	} else {
		/* Decode UTF-8 on the fly */
		uint32_t byte = (unsigned char)ch;
		
		if (format_char_utf8_state == 0 && byte < 128) {
			/* Fast path: ASCII */
			window_put_char(byte);
		} else {
			/* UTF-8 multibyte sequence */
			uint32_t type = utf8_decode(&format_char_utf8_state, &format_char_utf8_codepoint, byte);
			
			if (type == UTF8_ACCEPT) {
				/* Complete codepoint */
				window_put_char(format_char_utf8_codepoint);
				format_char_utf8_codepoint = 0;
			} else if (type == UTF8_REJECT) {
				/* Error - output replacement character or skip */
				format_char_utf8_state = UTF8_ACCEPT;
				format_char_utf8_codepoint = 0;
			}
			/* else: waiting for more bytes */
		}
	}
}

