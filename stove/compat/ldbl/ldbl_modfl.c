/* OpenBrowser. Copyright (c) 2026 Dalsin Limited. MIT licence. */
/* AmigaChrome os32-gcc16: long double via double (libnix has none). */
#include <math.h>
long double modfl(long double x, long double* ip) { double i; double r = modf((double)x, &i); *ip = i; return r; }
