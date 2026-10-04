/* OpenBrowser. Copyright (c) 2026 Dalsin Limited. MIT licence. */
/* AmigaChrome os32-gcc16: long double via double (libnix has none). */
#include <math.h>
long double ldexpl(long double x, int e) { return ldexp((double)x, e); }
