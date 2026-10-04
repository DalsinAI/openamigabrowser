# OpenBrowser's WebCore programs, built inside the WebKit build tree
# (Source/PlatformAmiga.cmake adds this folder when OPENBROWSER_WEBCORE_DIR
# names it), so they share WebKit's flags and link its static libraries.
# MIT, Copyright (c) 2026 Dalsin Limited.

set(OB_THIRD_PARTY "${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/openmail")
set(OB_AMISSL_SDK "$ENV{AMISSL_SDK}")
if (NOT EXISTS "${OB_AMISSL_SDK}/include/proto/amissl.h")
    set(OB_AMISSL_SDK "$ENV{HOME}/ACNet-compat-lab/toolchain/amiga/AmiSSL/Developer")
endif ()
set(OB_AMISSL_STUBS "${OB_AMISSL_SDK}/lib/AmigaOS3/libamisslstubs.a")

# The page layer: WebCore's clients, the view and its C interface.
set(OB_CORE_SOURCES
    AmigaBackForwardList.cpp
    AmigaChromeClient.cpp
    AmigaCookieJar.cpp
    AmigaEditorClient.cpp
    AmigaFrameLoaderClient.cpp
    AmigaGlobals.c
    AmigaLoaderStrategy.cpp
    AmigaNetworkSetup.c
    AmigaPlatformStrategies.cpp
    AmigaWebView.cpp
    ob_webview.cpp
    ${OB_THIRD_PARTY}/oam_stack.c
)

set(OB_FRAMEWORKS
    JavaScriptCore
    PAL
    WTF
    WebCore
    bmalloc
)
set(OB_PRIVATE_INCLUDE_DIRECTORIES
    ${CMAKE_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${OB_THIRD_PARTY}
    # AmigaNetwork.h: the Amiga network layer's hooks (not a public header)
    ${WEBCORE_DIR}/platform/network/amiga
)
set(OB_SYSTEM_INCLUDE_DIRECTORIES
    ${AC_WEBCORE_DEPS}/include
    ${AC_WEBCORE_DEPS}/include/cairo
    ${AC_WEBCORE_DEPS}/include/freetype2
    ${AC_WEBCORE_DEPS}/include/harfbuzz
    ${AC_WEBCORE_DEPS}/include/libxml2
    ${AC_WEBCORE_DEPS}/include/pixman-1
    ${OB_AMISSL_SDK}/include
)

# Static archives that depend on each other (WebCore, JavaScriptCore, WTF,
# cairo, fontconfig...) are linked as one group, so their order does not
# matter, followed by what libnix programs need.
macro(OB_LINK _target)
    target_link_directories(${_target} PRIVATE ${AC_WEBCORE_DEPS}/lib ${AC_WEBKIT_DEPS}/lib)
    target_link_libraries(${_target} PRIVATE
        -Wl,--start-group
        WebCore PAL JavaScriptCore WTF bmalloc
        cairo pixman-1 fontconfig freetype harfbuzz harfbuzz-icu expat png z xml2 sqlite3 curl psl
        icui18n icuuc icudata
        ${OB_AMISSL_STUBS}
        amiga pthread atomic stdc++ m
        -Wl,--end-group
    )
endmacro()

# obcore-dump: load a local HTML file, print its render tree (milestone 2a)
# and paint it into a PNG (2b).
set(obcore-dump_SOURCES
    AmigaGlobals.c
    AmigaPlatformStrategies.cpp
    DumpMain.cpp
    ${OB_THIRD_PARTY}/oam_stack.c
)
set(obcore-dump_FRAMEWORKS ${OB_FRAMEWORKS})
set(obcore-dump_PRIVATE_INCLUDE_DIRECTORIES ${OB_PRIVATE_INCLUDE_DIRECTORIES})
set(obcore-dump_SYSTEM_INCLUDE_DIRECTORIES ${OB_SYSTEM_INCLUDE_DIRECTORIES})
WEBKIT_EXECUTABLE_DECLARE(obcore-dump)
WEBKIT_EXECUTABLE(obcore-dump)
OB_LINK(obcore-dump)

# obcore-view: the page layer without a window, through its C interface.
set(obcore-view_SOURCES
    ${OB_CORE_SOURCES}
    obcore_view.c
)
set(obcore-view_FRAMEWORKS ${OB_FRAMEWORKS})
set(obcore-view_PRIVATE_INCLUDE_DIRECTORIES ${OB_PRIVATE_INCLUDE_DIRECTORIES})
set(obcore-view_SYSTEM_INCLUDE_DIRECTORIES ${OB_SYSTEM_INCLUDE_DIRECTORIES})
WEBKIT_EXECUTABLE_DECLARE(obcore-view)
WEBKIT_EXECUTABLE(obcore-view)
OB_LINK(obcore-view)

# OpenBrowser: the browser window (GadTools) around the page layer.
set(OpenBrowser_SOURCES
    ${OB_CORE_SOURCES}
    ob_blit.c
    ob_browser.c
)
set(OpenBrowser_FRAMEWORKS ${OB_FRAMEWORKS})
set(OpenBrowser_PRIVATE_INCLUDE_DIRECTORIES ${OB_PRIVATE_INCLUDE_DIRECTORIES})
set(OpenBrowser_SYSTEM_INCLUDE_DIRECTORIES ${OB_SYSTEM_INCLUDE_DIRECTORIES})
WEBKIT_EXECUTABLE_DECLARE(OpenBrowser)
WEBKIT_EXECUTABLE(OpenBrowser)
OB_LINK(OpenBrowser)
