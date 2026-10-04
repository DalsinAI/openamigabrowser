/*
 * OpenBrowser: bsdsocket.library and AmiSSL for WebCore's network thread
 * (AmigaNetworkSetup.c).
 * MIT, Copyright (c) 2026 Dalsin Limited.
 */
#ifndef OB_NETWORK_H
#define OB_NETWORK_H

#ifdef __cplusplus
extern "C" {
#endif

/* On the main task, before WebCore loads anything from the network. */
int ob_network_open(void);
void ob_network_close(void);

/* On the network thread, at its start and end. */
void ob_network_thread_started(void);
void ob_network_thread_finished(void);

#ifdef __cplusplus
}
#endif

#endif
