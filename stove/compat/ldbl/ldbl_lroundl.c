/* OpenBrowser. Copyright (c) 2026 AmigaChrome Team. MIT licence. */
/* AmigaChrome os32-gcc16: long double via double (libnix has none). */
#include <math.h>
long lroundl(long double x) { return lround((double)x); }
