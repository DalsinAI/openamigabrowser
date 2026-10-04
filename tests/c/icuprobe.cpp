#include <cstdio>
#include <unicode/ucol.h>
#include <unicode/uchar.h>
#include <unicode/ustring.h>
#include <unicode/uversion.h>
int main()
{
    setvbuf(stdout, nullptr, _IONBF, 0);
    std::printf("ICUPROBE_START\n");
    UVersionInfo v; char vs[U_MAX_VERSION_STRING_LENGTH]; u_getVersion(v); u_versionToString(v, vs);
    std::printf("icu=%s upper(a)=%c\n", vs, (char)u_toupper('a'));
    UErrorCode e = U_ZERO_ERROR;
    std::printf("calling ucol_open\n");
    UCollator* c = ucol_open("de", &e);
    std::printf("ucol_open de: %s\n", u_errorName(e));
    if (c) {
        UChar a[] = { 0x00E4, 0 }, b[] = { 'b', 0 };
        std::printf("collate a-umlaut < b: %d\n", ucol_strcoll(c, a, -1, b, -1) == UCOL_LESS);
        ucol_close(c);
    }
    std::printf("ICUPROBE_OK\n");
}
