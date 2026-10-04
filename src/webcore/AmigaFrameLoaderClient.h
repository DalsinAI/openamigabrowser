/*
 * OpenBrowser: WebCore's frame loader client (one per frame).
 * MIT, Copyright (c) 2026 Dalsin Limited.
 */
#pragma once

#include <wtf/UniqueRef.h>

namespace WebCore {
class FrameLoader;
class LocalFrameLoaderClient;
}

namespace OpenBrowser {

class WebView;

UniqueRef<WebCore::LocalFrameLoaderClient> createFrameLoaderClient(WebCore::FrameLoader&, WebView&);

} // namespace OpenBrowser
