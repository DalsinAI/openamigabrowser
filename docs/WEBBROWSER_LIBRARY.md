# webbrowser.library

Web pages for any Amiga program: a URL in, a page ready to use out (its
picture as 32-bit pixels or drawn into a RastPort, its text and its title).

```c
struct Library *WebBrowserBase = OpenLibrary("webbrowser.library", 1);
APTR view = WB_OpenView(640, 480, NULL);
WB_Load(view, "https://www.bbc.co.uk/");
WB_WaitLoaded(view, 120);                       /* seconds at most */
WB_RenderRastPort(view, window->RPort, 0, 0, 0, 0, 640, 480);
WB_CloseView(view);
CloseLibrary(WebBrowserBase);
```

`src/webbrowser/wbgrab.c` (WBGrab) is the worked example: it saves a page's
picture and text from the Shell.

## How it works

The library is 4 KB. The browser engine (WebCore, JavaScriptCore, the
network) is a separate program, `LIBS:WebBrowser/WebBrowserEngine`, that
the library starts the first time a program needs it. It opens the public
port `WEBBROWSER.ENGINE`; every library call is one message to it, answered
when the engine has done the work, so calls are synchronous and can come
from any task. The engine keeps running after the last program closes the
library, so the next program finds it ready instead of waiting for 95 MB to
load. `WB_Shutdown()` ends it.

Between messages the engine runs WebCore's timers and the network, so pages
go on loading while programs do other things. `WBA_Signal` asks for a signal
when a page changes or finishes.

If the engine crashes, the programs waiting on it get `WBERR_NOENGINE`, and
the engine prints the code offsets of the crash (for its link map) on its
output.

## Installing

```
LIBS:webbrowser.library
LIBS:WebBrowser/WebBrowserEngine
LIBS:WebBrowser/Fonts/                 the TrueType fonts
LIBS:WebBrowser/fontconfig/fonts.conf  (its dir is PROGDIR:Fonts)
```

The engine keeps its cookies in `LIBS:WebBrowser/Cookies.db` and its TLS
sessions in `LIBS:WebBrowser/TLSSessions`. Without the fonts it stops at
the first page.

## Calls

| Call | Offset | |
| --- | --- | --- |
| `WB_OpenView(width, height, tags)` | -30 | a view; tags `WBA_Scripts`, `WBA_Pictures`, `WBA_Signal` |
| `WB_CloseView(view)` | -36 | |
| `WB_Load(view, url)` | -42 | starts loading |
| `WB_LoadHTML(view, html, baseURL)` | -48 | |
| `WB_State(view)` | -54 | `WBS_LOADING`, `WBS_FAILED`, `WBS_CHANGED` |
| `WB_WaitLoaded(view, seconds)` | -60 | 0 loaded, 1 failed, `WBERR_TIMEOUT` |
| `WB_Render(view, argb, stride, x, y, w, h)` | -66 | ARGB32 pixels of the page |
| `WB_RenderRastPort(view, rp, pageX, pageY, x, y, w, h)` | -72 | needs cybergraphics.library (RTG) |
| `WB_GetText(view, buffer, size)` | -78 | the page's text, UTF-8; returns its full length |
| `WB_GetTitle(view, buffer, size)` | -84 | |
| `WB_GetURL(view, buffer, size)` | -90 | |
| `WB_Resize(view, width, height)` | -96 | |
| `WB_Mouse(view, type, x, y, button)` | -102 | `WBM_MOVE`, `WBM_DOWN`, `WBM_UP` |
| `WB_Key(view, down, rawKey, text, qualifiers)` | -108 | |
| `WB_Scroll(view, dx, dy)` | -114 | |
| `WB_Shutdown()` | -120 | ends the engine |

Headers: `src/webbrowser/include` (`libraries/webbrowser.h`,
`proto/webbrowser.h`, `inline/webbrowser.h`, `clib/webbrowser_protos.h`).

## Building

`src/webbrowser/build-library.sh [outdir]` builds the library and WBGrab with
the os32-gcc16 compiler. The engine is a WebCore target:
`ninja WebBrowserEngine` in the WebCore build tree
(`scripts/build-webcore.sh build-browser` builds it with the browser).
