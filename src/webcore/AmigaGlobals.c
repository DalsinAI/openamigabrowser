/*
 * Library bases that the static libraries call through: curl reaches
 * bsdsocket.library through SocketBase, and OpenSSL calls go through
 * AmiSSL's stub library to AmiSSLBase and AmiSSLExtBase. Each task that uses
 * the network opens its own (see the network code); programs without a
 * network leave them empty.
 * MIT, Copyright (c) 2026 Dalsin Limited.
 */
#include <exec/libraries.h>

struct Library *SocketBase;
struct Library *AmiSSLMasterBase;
struct Library *AmiSSLBase;
struct Library *AmiSSLExtBase;

#include <dos/dosextens.h>
#include <proto/exec.h>

/* No "Please insert volume" requesters for this program: a page, a font
 * setting or a library asking for a name that is not mounted gets an error
 * at once instead of a window waiting for someone to answer it. */
void ob_quiet_requesters(void)
{
    struct Process *me = (struct Process *)FindTask(NULL);
    if (me->pr_Task.tc_Node.ln_Type == NT_PROCESS)
        me->pr_WindowPtr = (APTR)-1;
}
