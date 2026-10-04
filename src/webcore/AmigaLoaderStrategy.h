/*
 * OpenBrowser: WebCore's loader strategy with the network (see
 * AmigaLoaderStrategy.cpp, LGPL 2 or later).
 */
#pragma once

namespace WebCore {
class LoaderStrategy;
}

namespace OpenBrowser {

WebCore::LoaderStrategy* networkLoaderStrategy();

} // namespace OpenBrowser
