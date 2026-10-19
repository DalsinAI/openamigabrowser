/*
 * OpenBrowser: bsdsocket.library for WebCore's network thread.
 *
 * On AmigaOS a task uses its own bsdsocket.library base. The main task opens
 * one (WebCore sets up curl there); curl's thread, which makes every
 * connection, opens its own socket base when it starts. Only that thread
 * touches sockets, so the program's one SocketBase is its base while it
 * runs. Each base points bsdsocket's errno at the C library's.
 *
 * TLS is curl's: openamigacurl's OpenTLS backend opens opentls.library (1.2
 * or newer) itself, and checks certificates against S:OpenTLS/ca-bundle.pem.
 * Until 19 October 2026 this file also opened AmiSSL in each task.
 *
 * MIT, Copyright (c) 2026 AmigaChrome Team.
 */
#include <errno.h>
#include <exec/types.h>
#include <utility/tagitem.h>
#include <proto/exec.h>
#include <proto/bsdsocket.h>
#ifdef OB_OPENSERVICE
#include "ob_media.h"
#endif

#include "ob_network.h"

extern struct Library *SocketBase;

static struct Library *mainSocketBase, *threadSocketBase;
static BYTE mainPriority;

/* ROM math libraries opened on the way can leave the FPU in single precision. */
static void resetFPCR(void)
{
    __asm__ volatile ("fmove.l %0,%%fpcr" : : "d" (0));
}

int ob_network_open(void)
{
    mainPriority = FindTask(NULL)->tc_Node.ln_Pri;
    mainSocketBase = OpenLibrary((CONST_STRPTR)"bsdsocket.library", 4);
    if (!mainSocketBase)
        return 0;
    SocketBase = mainSocketBase;
    SocketBaseTags(SBTM_SETVAL(SBTC_ERRNOPTR(sizeof(errno))), (ULONG)&errno, TAG_DONE);
    resetFPCR();
#ifdef OB_OPENSERVICE
    /* Page pictures, SVG included, decoded by a board in this machine or a
     * paired Cradle, when one offers it. */
    ob_media_open();
#endif
    return 1;
}

void ob_network_close(void)
{
#ifdef OB_OPENSERVICE
    ob_media_close();
#endif
    if (mainSocketBase)
        CloseLibrary(mainSocketBase);
    SocketBase = mainSocketBase = NULL;
}

void ob_network_thread_started(void)
{
    /* One priority above the browser: a TLS handshake is seconds of
     * public-key arithmetic on a 68k, and a server hangs up on a client that
     * takes too long, so the page's work must not slow it down. The thread
     * spends most of its time waiting on its sockets. */
    SetTaskPri(FindTask(NULL), mainPriority + 1);
    threadSocketBase = OpenLibrary((CONST_STRPTR)"bsdsocket.library", 4);
    if (threadSocketBase) {
        SocketBase = threadSocketBase;
        SocketBaseTags(SBTM_SETVAL(SBTC_ERRNOPTR(sizeof(errno))), (ULONG)&errno, TAG_DONE);
    }
    resetFPCR();
}

void ob_network_thread_finished(void)
{
    if (threadSocketBase)
        CloseLibrary(threadSocketBase);
    threadSocketBase = NULL;
    SocketBase = mainSocketBase;
}
