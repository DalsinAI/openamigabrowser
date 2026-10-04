#include <stdio.h>
volatile double a = 0.1, b = 0.2;
int main(void)
{
    unsigned int fpcr;
    double s = a + b;
    __asm__ __volatile__("fmove.l %%fpcr,%0" : "=d"(fpcr));
    printf("fpcr=%08x sum=%.17g third=%.17g\n", fpcr, s, a / 0.3);
    return 0;
}
