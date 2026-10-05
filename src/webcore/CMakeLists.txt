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

# The page layer: WebCore's clients, the view and its C interface, built
# once as the static library OBCore and linked into each program. A change
# here compiles that file and relinks; it never compiles WebCore.
set(OB_CORE_SOURCES
    AmigaBackForwardList.cpp
    AmigaChromeClient.cpp
    AmigaCookieJar.cpp
    AmigaEditorClient.cpp
    AmigaFrameLoaderClient.cpp
    AmigaLoaderStrategy.cpp
    AmigaNetworkSetup.c
    AmigaPlatformStrategies.cpp
    AmigaWebView.cpp
    ob_webview.cpp
)
# Each program compiles these itself: they define symbols (library bases,
# the stack size) that the C library also has, and an object in a program
# always wins over one in a library.
set(OB_PROGRAM_SOURCES
    AmigaGlobals.c
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
    # A link map beside each program (bin/NAME.map): which object every
    # address belongs to, static functions included, which the program's
    # own symbol table leaves out. For profiles, and for where the size goes.
    target_link_options(${_target} PRIVATE -Wl,-Map=${CMAKE_BINARY_DIR}/bin/${_target}.map)
    target_link_libraries(${_target} PRIVATE
        -Wl,--start-group
        OBCore WebCore PAL JavaScriptCore WTF bmalloc
        cairo pixman-1 fontconfig freetype harfbuzz harfbuzz-icu expat png z xml2 sqlite3 curl psl
        icui18n icuuc icudata
        ${OB_AMISSL_STUBS}
        amiga pthread atomic stdc++ m
        -Wl,--end-group
    )
endmacro()

set(OBCore_SOURCES ${OB_CORE_SOURCES})
set(OBCore_LIBRARY_TYPE STATIC)
set(OBCore_FRAMEWORKS ${OB_FRAMEWORKS})
set(OBCore_PRIVATE_INCLUDE_DIRECTORIES ${OB_PRIVATE_INCLUDE_DIRECTORIES})
set(OBCore_SYSTEM_INCLUDE_DIRECTORIES ${OB_SYSTEM_INCLUDE_DIRECTORIES})
# Compile against WebKit as the programs do: linking the frameworks brings
# their include folders and definitions. WebCore's generated and private
# headers must exist before OBCore compiles.
set(OBCore_PRIVATE_LIBRARIES WebKit::WebCore WebKit::PAL WebKit::JavaScriptCore WebKit::WTF WebKit::bmalloc)
set(OBCore_DEPENDENCIES WebCore)
WEBKIT_LIBRARY_DECLARE(OBCore)
WEBKIT_LIBRARY(OBCore)

# obcore-dump: load a local HTML file, print its render tree (milestone 2a)
# and paint it into a PNG (2b).
set(obcore-dump_SOURCES
    ${OB_PROGRAM_SOURCES}
    DumpMain.cpp
)
set(obcore-dump_FRAMEWORKS ${OB_FRAMEWORKS})
set(obcore-dump_PRIVATE_INCLUDE_DIRECTORIES ${OB_PRIVATE_INCLUDE_DIRECTORIES})
set(obcore-dump_SYSTEM_INCLUDE_DIRECTORIES ${OB_SYSTEM_INCLUDE_DIRECTORIES})
WEBKIT_EXECUTABLE_DECLARE(obcore-dump)
WEBKIT_EXECUTABLE(obcore-dump)
OB_LINK(obcore-dump)

# obcore-view: the page layer without a window, through its C interface.
set(obcore-view_SOURCES
    ${OB_PROGRAM_SOURCES}
    obcore_view.c
)
set(obcore-view_FRAMEWORKS ${OB_FRAMEWORKS})
set(obcore-view_PRIVATE_INCLUDE_DIRECTORIES ${OB_PRIVATE_INCLUDE_DIRECTORIES})
set(obcore-view_SYSTEM_INCLUDE_DIRECTORIES ${OB_SYSTEM_INCLUDE_DIRECTORIES})
WEBKIT_EXECUTABLE_DECLARE(obcore-view)
WEBKIT_EXECUTABLE(obcore-view)
OB_LINK(obcore-view)

# OpenBrowser.engine: the browser window (GadTools) around the page layer.
# The launcher below loads and runs it.
set(OpenBrowser_SOURCES
    ${OB_PROGRAM_SOURCES}
    ob_blit.c
    ob_browser.c
    ob_splash.c
)
set(OpenBrowser_OUTPUT_NAME OpenBrowser.engine)
set(OpenBrowser_FRAMEWORKS ${OB_FRAMEWORKS})
set(OpenBrowser_PRIVATE_INCLUDE_DIRECTORIES ${OB_PRIVATE_INCLUDE_DIRECTORIES})
set(OpenBrowser_SYSTEM_INCLUDE_DIRECTORIES ${OB_SYSTEM_INCLUDE_DIRECTORIES})
WEBKIT_EXECUTABLE_DECLARE(OpenBrowser)
WEBKIT_EXECUTABLE(OpenBrowser)
OB_LINK(OpenBrowser)

# OpenBrowser: the small program the icon starts. It shows a title window at
# once, loads OpenBrowser.engine (about 100 MB) with a progress bar and runs
# it in its own process. It needs nothing of WebKit's, only the C library,
# the stove's compat library (the toolchain wraps malloc) and libpthread
# (the toolchain links with -fexceptions, whose frame tables use it).
add_executable(OpenBrowserLauncher ob_launcher.c ob_splash.c)
set_target_properties(OpenBrowserLauncher PROPERTIES OUTPUT_NAME OpenBrowser)
target_link_libraries(OpenBrowserLauncher PRIVATE -L${AC_OS32_STOVE}/compat -los32compat pthread)
