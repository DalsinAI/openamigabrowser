/*
 * OpenBrowser: WebCore's editor client (typing into form fields).
 * MIT, Copyright (c) 2026 Dalsin Limited.
 */
#pragma once

#include <wtf/UniqueRef.h>

namespace WebCore {
class EditorClient;
}

namespace OpenBrowser {

UniqueRef<WebCore::EditorClient> createEditorClient();

} // namespace OpenBrowser
