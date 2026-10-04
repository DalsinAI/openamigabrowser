/* OpenBrowser (DalsinAI/openamigabrowser). Copyright (c) 2026 Dalsin Limited. MIT licence, see LICENSE. */
/* Minimal C11 <uchar.h> for AmigaOS 3.x (AmigaChrome os32-gcc16).
 * Types only; libnix has no mbrtoc16/c16rtomb. In C++ the types are built in. */
#ifndef _AMIGACHROME_UCHAR_H
#define _AMIGACHROME_UCHAR_H
#include <stddef.h>
#include <stdint.h>
#ifndef __cplusplus
typedef uint_least16_t char16_t;
typedef uint_least32_t char32_t;
#endif
#endif
