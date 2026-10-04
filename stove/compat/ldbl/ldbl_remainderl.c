/* OpenBrowser. Copyright (c) 2026 Dalsin Limited. MIT licence. */
/* AmigaChrome os32-gcc16: long double via double (libnix has none). */
#include <math.h>
long double remainderl(long double x, long double y) { return remainder((double)x, (double)y); }
