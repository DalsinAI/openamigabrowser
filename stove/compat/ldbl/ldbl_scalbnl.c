/* OpenBrowser. Copyright (c) 2026 AmigaChrome Team. MIT licence. */
/* AmigaChrome os32-gcc16: long double via double (libnix has none). */
#include <math.h>
long double scalbnl(long double x, int e) { return scalbn((double)x, e); }
