/*
 * OpenBrowser: WebCore's platform strategies (loader, blobs, pasteboard).
 * MIT, Copyright (c) 2026 Dalsin Limited.
 */
#pragma once

namespace WebCore {
class LoaderStrategy;
}

namespace OpenBrowser {

// Installs the strategies; call once, after JSC::initialize(). Without a
// loader strategy, pages load nothing from the network.
void initializePlatformStrategies(WebCore::LoaderStrategy* = nullptr);

} // namespace OpenBrowser
