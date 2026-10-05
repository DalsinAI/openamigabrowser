/*
 * OpenBrowser: bsdsocket.library and AmiSSL for WebCore's network thread.
 *
 * On AmigaOS a task uses its own bsdsocket.library base, and each task that
 * calls AmiSSL joins it first. The main task opens both (WebCore sets up
 * curl and OpenSSL there); curl's thread, which makes every connection,
 * opens its own socket base and joins AmiSSL when it starts. Only that
 * thread touches sockets, so the program's one SocketBase is its base while
 * it runs.
 *
 * MIT, Copyright (c) 2026 Dalsin Limited.
 */
#include <errno.h>
#include <exec/types.h>
#include <utility/tagitem.h>
#include <proto/exec.h>
#include <proto/bsdsocket.h>
#include <proto/amisslmaster.h>
#include <proto/amissl.h>
#include <libraries/amisslmaster.h>
#include <libraries/amissl.h>
#include <amissl/amissl.h>
#ifdef OB_OPENTLS
#include "opentls_amiga.h"
#include "ob_media.h"
#endif

#include "ob_network.h"

extern struct Library *SocketBase, *AmiSSLMasterBase, *AmiSSLBase, *AmiSSLExtBase;

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
    AmiSSLMasterBase = OpenLibrary((CONST_STRPTR)"amisslmaster.library", AMISSLMASTER_MIN_VERSION);
    if (!AmiSSLMasterBase) {
        ob_network_close();
        return 0;
    }
    if (OpenAmiSSLTags(AMISSL_CURRENT_VERSION, AmiSSL_UsesOpenSSLStructs, TRUE, AmiSSL_GetAmiSSLBase, (ULONG)&AmiSSLBase,
            AmiSSL_GetAmiSSLExtBase, (ULONG)&AmiSSLExtBase, AmiSSL_SocketBase, (ULONG)SocketBase,
            AmiSSL_ErrNoPtr, (ULONG)&errno, TAG_DONE)) {
        AmiSSLBase = NULL;
        ob_network_close();
        return 0;
    }
    resetFPCR();
#ifdef OB_OPENTLS
    /* TLS key maths to a board in this machine or a paired Cradle, when one
     * offers it; AmiSSL does it all itself otherwise. */
    opentls_amiga_open();
    /* Page pictures, SVG included, decoded there too. */
    ob_media_open();
#endif
    return 1;
}

void ob_network_close(void)
{
#ifdef OB_OPENTLS
    ob_media_close();
    opentls_amiga_close();
#endif
    if (AmiSSLBase)
        CloseAmiSSL();
    if (AmiSSLMasterBase)
        CloseLibrary(AmiSSLMasterBase);
    if (mainSocketBase)
        CloseLibrary(mainSocketBase);
    AmiSSLBase = AmiSSLExtBase = AmiSSLMasterBase = NULL;
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
    if (AmiSSLBase)
        InitAmiSSL(AmiSSL_ErrNoPtr, (ULONG)&errno, AmiSSL_SocketBase, (ULONG)SocketBase, TAG_DONE);
    resetFPCR();
}

void ob_network_thread_finished(void)
{
    if (AmiSSLBase)
        CleanupAmiSSL(TAG_DONE);
    if (threadSocketBase)
        CloseLibrary(threadSocketBase);
    threadSocketBase = NULL;
    SocketBase = mainSocketBase;
}
