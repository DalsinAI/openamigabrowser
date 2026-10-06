/*
 * OpenBrowser: one web page in a view: WebCore's Page with OpenBrowser's
 * clients, loading, painting into a 32-bit ARGB buffer, and input. The
 * Amiga window around it talks to it through ob_webview.h.
 * MIT, Copyright (c) 2026 Dalsin Limited.
 */
#pragma once

#include "ob_webview.h"

#include <WebCore/IntRect.h>
#include <WebCore/IntSize.h>
#include <WebCore/Timer.h>
#include <wtf/Forward.h>
#include <wtf/RefPtr.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/text/WTFString.h>

namespace WebCore {
class LocalFrame;
class LocalFrameView;
class Page;
class ResourceError;
}

namespace OpenBrowser {

class AmigaBackForwardList;

class WebView {
    WTF_MAKE_NONCOPYABLE(WebView);
    WTF_MAKE_TZONE_ALLOCATED(WebView);
public:
    WebView(const OBWebViewCallbacks&, const WebCore::IntSize&);
    ~WebView();

    WebCore::Page& page() { return *m_page; }
    WebCore::LocalFrame* mainFrame() const;
    WebCore::LocalFrameView* mainFrameView() const;
    const WebCore::IntSize& size() const { return m_size; }

    // From the window.
    void load(const String& url);
    void loadHTML(const String& html, const String& baseURL);
    void goBack();
    void goForward();
    void reload();
    void stop();
    bool canGoBack() const;
    bool canGoForward() const;
    void resize(const WebCore::IntSize&);
    void setScriptsEnabled(bool);
    void setPicturesEnabled(bool);
    void setWebFontsEnabled(bool);
    void setLiteMode(bool enabled) { m_liteMode = enabled; }
    bool liteMode() const { return m_liteMode; }
    void paint(unsigned char* argb, int stride, const WebCore::IntRect&);
    void reportDisplayList(unsigned char* direct, unsigned char* replayed, int stride, const WebCore::IntRect&);
    WebCore::IntRect takeDirtyRect();

    // From WebCore's clients.
    void invalidate(const WebCore::IntRect&);
    void scheduleRenderingUpdate();
    void setStatusText(const String&);
    void setTitle(const String&);
    void didCommitLoad(const String& url);
    void didStartLoad();
    void didFinishLoad();
    void didFailLoad(const WebCore::ResourceError&);
    void setProgress(double);
    void contentsSizeChanged(const WebCore::IntSize&);
    void runAlert(const String&);
    bool runConfirm(const String&);
    bool runPrompt(const String& message, const String& defaultValue, String& result);
    void consoleMessage(const String& message, unsigned line, const String& source);
    void resourceStarted(const String& url);
    void resourceEnded(const String& url, const String& error);

private:
    void renderingUpdateTimerFired();
    void callString(void (*callback)(void*, const char*), const String&);

    OBWebViewCallbacks m_callbacks;
    WebCore::IntSize m_size;
    RefPtr<WebCore::Page> m_page;
    RefPtr<AmigaBackForwardList> m_backForwardList;
    WebCore::IntRect m_dirtyRect;
    WebCore::Timer m_renderingUpdateTimer;
    MonotonicTime m_nextRenderingUpdate; // while a page loads, none before this
    bool m_loading { false };
    bool m_liteMode { false };
};

} // namespace OpenBrowser
