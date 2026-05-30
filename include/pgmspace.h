/**
 * @file pgmspace.h
 * @brief AVR pgmspace.h compatibility shim for ARM-based platforms.
 *
 * On AVR (and some other MCUs), pgmspace.h provides macros for storing data
 * in program memory (flash). ARM Cortex-M has a unified address space, so
 * these macros are simply defined as no-ops here.
 *
 * This header is required because some libraries (e.g. FastCRC) unconditionally
 * include <pgmspace.h> when building for Arduino targets, even on ARM platforms
 * that do not provide it through their framework (e.g. Renesas RA4M1).
 */

#pragma once

#ifndef PGMSPACE_H
#define PGMSPACE_H

#include <stdint.h>

/* ---- Attribute / qualifier no-ops ---- */
#ifndef PROGMEM
#define PROGMEM
#endif

#ifndef PSTR
#define PSTR(s) (s)
#endif

#ifndef PGM_P
#define PGM_P const char *
#endif

#ifndef PGM_VOID_P
#define PGM_VOID_P const void *
#endif

/* ---- pgm_read_* macros ---- */
#ifndef pgm_read_byte
#define pgm_read_byte(addr)   (*(const uint8_t  *)(addr))
#endif

#ifndef pgm_read_word
#define pgm_read_word(addr)   (*(const uint16_t *)(addr))
#endif

#ifndef pgm_read_dword
#define pgm_read_dword(addr)  (*(const uint32_t *)(addr))
#endif

#ifndef pgm_read_float
#define pgm_read_float(addr)  (*(const float    *)(addr))
#endif

#ifndef pgm_read_ptr
#define pgm_read_ptr(addr)    (*(const void * const *)(addr))
#endif

/* ---- pgm_read_byte_near / far (aliases) ---- */
#ifndef pgm_read_byte_near
#define pgm_read_byte_near(addr)  pgm_read_byte(addr)
#endif

#ifndef pgm_read_word_near
#define pgm_read_word_near(addr)  pgm_read_word(addr)
#endif

#ifndef pgm_read_dword_near
#define pgm_read_dword_near(addr) pgm_read_dword(addr)
#endif

#ifndef pgm_read_byte_far
#define pgm_read_byte_far(addr)   pgm_read_byte(addr)
#endif

#ifndef pgm_read_word_far
#define pgm_read_word_far(addr)   pgm_read_word(addr)
#endif

#ifndef pgm_read_dword_far
#define pgm_read_dword_far(addr)  pgm_read_dword(addr)
#endif

/* ---- String functions (redirect to standard libc) ---- */
#include <string.h>

#ifndef strlen_P
#define strlen_P(s)           strlen(s)
#endif

#ifndef strcpy_P
#define strcpy_P(d, s)        strcpy(d, s)
#endif

#ifndef strncpy_P
#define strncpy_P(d, s, n)   strncpy(d, s, n)
#endif

#ifndef strcmp_P
#define strcmp_P(a, b)        strcmp(a, b)
#endif

#ifndef strncmp_P
#define strncmp_P(a, b, n)   strncmp(a, b, n)
#endif

#ifndef strcasecmp_P
#define strcasecmp_P(a, b)    strcasecmp(a, b)
#endif

#ifndef strstr_P
#define strstr_P(h, n)        strstr(h, n)
#endif

#ifndef memcpy_P
#define memcpy_P(d, s, n)    memcpy(d, s, n)
#endif

#ifndef memcmp_P
#define memcmp_P(a, b, n)    memcmp(a, b, n)
#endif

/* ---- printf / sprintf variants ---- */
#ifndef printf_P
#define printf_P              printf
#endif

#ifndef sprintf_P
#define sprintf_P             sprintf
#endif

#ifndef snprintf_P
#define snprintf_P            snprintf
#endif

#ifndef vsnprintf_P
#define vsnprintf_P           vsnprintf
#endif

#endif /* PGMSPACE_H */
