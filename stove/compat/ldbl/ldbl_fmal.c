/* OpenBrowser. Copyright (c) 2026 Dalsin Limited. MIT licence. */
/* AmigaChrome os32-gcc16: long double via double (libnix has none). */
#include <math.h>
long double fmal(long double x, long double y, long double z) { return fma((double)x, (double)y, (double)z); }
