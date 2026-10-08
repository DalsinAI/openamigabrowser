/* oam_net on AmigaOS 3.x / AROS 68k: bsdsocket.library + OpenTLS.
 * Socket ownership stays here; OpenTLS receives only read/write callbacks. */
#include "oam_net.h"
#include <opentls/opentls.h>

#include <proto/exec.h>
#include <proto/bsdsocket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct Library *SocketBase;
void (*oam_net_progress)(const char *step);
#define STEP(s) do { if (oam_net_progress) oam_net_progress(s); } while (0)

struct oam_conn {
    long fd;
    OTContext *tls_ctx;
    OTConnection *tls;
    int timeout;
    char error[200];
};

static void set_err(char *err, size_t errlen, const char *what)
{
    if (err && errlen) snprintf(err, errlen, "%s", what);
}

static long tls_recv(void *ctx, void *buf, long len)
{
    struct oam_conn *c = (struct oam_conn *)ctx;
    return recv(c->fd, buf, len, 0);
}

static long tls_send(void *ctx, const void *buf, long len)
{
    struct oam_conn *c = (struct oam_conn *)ctx;
    return send(c->fd, (APTR)buf, len, 0);
}

int oam_net_init(char *err, size_t errlen)
{
    if (SocketBase) return 1;
    STEP("opening bsdsocket.library");
    if (!(SocketBase = OpenLibrary("bsdsocket.library", 4))) {
        set_err(err, errlen, "No TCP/IP stack is running (bsdsocket.library).");
        return 0;
    }
    SocketBaseTags(SBTM_SETVAL(SBTC_ERRNOPTR(sizeof errno)), (ULONG)&errno, TAG_DONE);
    STEP("network ready (OpenTLS)");
    return 1;
}

void oam_net_cleanup(void)
{
    if (SocketBase) { CloseLibrary(SocketBase); SocketBase = NULL; }
}

void oam_net_set_timeout(oam_conn *c, int seconds) { c->timeout = seconds; }

static int start_tls(oam_conn *c, const char *host, char *err, size_t errlen)
{
    OTConfig cfg;
    int rc;
    memset(&cfg, 0, sizeof cfg);
    cfg.alpn = "http/1.1";
    cfg.min_tls = 12;
    STEP("OpenTLS context");
    c->tls_ctx = ot_context_new(&cfg, err, errlen);
    if (!c->tls_ctx) return 0;
    c->tls = ot_connection_new(c->tls_ctx, host, tls_recv, tls_send, c, err, errlen);
    if (!c->tls) return 0;
    STEP("OpenTLS handshake");
    rc = ot_connect(c->tls);
    if (rc != OT_OK) {
        set_err(err, errlen, ot_error(c->tls));
        return 0;
    }
    return 1;
}

oam_conn *oam_net_connect(const char *host, int port, int tls, char *err, size_t errlen)
{
    struct hostent *he;
    struct sockaddr_in sa;
    oam_conn *c;
    if (!oam_net_init(err, errlen)) return NULL;
    c = calloc(1, sizeof *c);
    if (!c) { set_err(err, errlen, "Out of memory."); return NULL; }
    c->fd = -1;
    c->timeout = 60;
    STEP("resolving the name");
    he = gethostbyname((STRPTR)host);
    if (!he || he->h_addrtype != AF_INET || !he->h_addr_list[0]) {
        char msg[200];
        snprintf(msg, sizeof msg, "The server %s was not found.", host);
        set_err(err, errlen, msg);
        free(c);
        return NULL;
    }
    memset(&sa, 0, sizeof sa);
    sa.sin_family = AF_INET;
    sa.sin_port = htons((unsigned short)port);
    memcpy(&sa.sin_addr, he->h_addr_list[0], sizeof sa.sin_addr);
    STEP("connecting");
    c->fd = socket(AF_INET, SOCK_STREAM, 0);
    if (c->fd < 0 || connect(c->fd, (struct sockaddr *)&sa, sizeof sa) < 0) {
        char msg[200];
        snprintf(msg, sizeof msg, "%s did not answer on port %d.", host, port);
        set_err(err, errlen, msg);
        oam_net_close(c);
        return NULL;
    }
    if (tls && !start_tls(c, host, err, errlen)) { oam_net_close(c); return NULL; }
    return c;
}

int oam_net_starttls(oam_conn *c, const char *host, char *err, size_t errlen)
{
    if (c->tls) return 1;
    return start_tls(c, host, err, errlen);
}

static int readable(oam_conn *c)
{
    fd_set rd;
    struct timeval tv;
    if (c->tls && ot_pending(c->tls) > 0) return 1;
    if (!c->timeout) return 1;
    FD_ZERO(&rd);
    FD_SET(c->fd, &rd);
    tv.tv_sec = c->timeout;
    tv.tv_usec = 0;
    return WaitSelect(c->fd + 1, &rd, NULL, NULL, &tv, NULL) > 0;
}

long oam_net_read(oam_conn *c, void *buf, long len)
{
    long n;
    if (!readable(c)) { snprintf(c->error, sizeof c->error, "The server stopped answering."); return -1; }
    n = c->tls ? ot_read(c->tls, buf, len) : recv(c->fd, buf, len, 0);
    if (n < 0)
        snprintf(c->error, sizeof c->error, "%s", c->tls ? ot_error(c->tls) : "Reading from the server failed.");
    return n;
}

long oam_net_write(oam_conn *c, const void *buf, long len)
{
    const char *p = (const char *)buf;
    long left = len;
    while (left > 0) {
        long n = c->tls ? ot_write(c->tls, p, left) : send(c->fd, (APTR)p, left, 0);
        if (n <= 0) {
            snprintf(c->error, sizeof c->error, "%s", c->tls ? ot_error(c->tls) : "Sending to the server failed.");
            return -1;
        }
        p += n;
        left -= n;
    }
    return len;
}

const char *oam_net_error(oam_conn *c) { return c->error[0] ? c->error : "No error."; }

void oam_net_close(oam_conn *c)
{
    if (!c) return;
    if (c->tls) { ot_shutdown(c->tls); ot_connection_free(c->tls); }
    if (c->tls_ctx) ot_context_free(c->tls_ctx);
    if (c->fd >= 0) CloseSocket(c->fd);
    free(c);
}
