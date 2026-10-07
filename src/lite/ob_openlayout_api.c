#ifdef OB_USE_OPENLAYOUT_LIBRARY
#include <exec/types.h>
#include <exec/libraries.h>
#include <proto/exec.h>
#include <proto/openlayout.h>

struct Library *OpenLayoutBase = NULL;

int ob_openlayout_open(void)
{
    if (OpenLayoutBase) return 1;
    OpenLayoutBase = OpenLibrary((CONST_STRPTR)OPENLAYOUTLIB_NAME,
                                 OPENLAYOUTLIB_VERSION);
    return OpenLayoutBase != NULL;
}

void ob_openlayout_close(void)
{
    if (OpenLayoutBase) {
        CloseLibrary(OpenLayoutBase);
        OpenLayoutBase = NULL;
    }
}
#endif
