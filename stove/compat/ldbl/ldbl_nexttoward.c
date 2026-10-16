/* OpenBrowser. Copyright (c) 2026 AmigaChrome Team. MIT licence. */
/* AmigaChrome os32-gcc16: long double via double (libnix has none). */
#include <math.h>
double nexttoward(double x, long double y) { return nextafter(x, (double)y); }
