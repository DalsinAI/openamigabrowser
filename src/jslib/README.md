# javascript.library

JavaScript for any AmigaOS 3.2 program on a 68040: JavaScriptCore with our
68k JIT, shared by every program that opens the library. The program owns
its objects and hands them to scripts as **host classes** (tables of
methods, accessors and fields); the library owns the scripts. Design:
[javascript.library: design](https://claude.ai/artifact/J3CieCdmqLrnL8QvJMfdwv).
Lives here for now; it moves to its own repository later.

```c
struct Library *JavaScriptBase = OpenLibrary("javascript.library", 1);
JSTask t = JS_NewTask(NULL);
JSValue v;
if (JS_Evaluate(t, "6 * 7", -1, "sum.js", &v) == JSERR_OK) {
    LONG n;
    JS_ToLong(t, v, &n);            /* 42 */
    JS_Free(t, v);
}
JS_EndTask(t);
CloseLibrary(JavaScriptBase);
```

From the Shell: `JS script.js arg1 arg2` or `JS EVAL "print(6 * 7)"`.

## How it is put together

| Part | What it is |
| --- | --- |
| `javascript_lib.c` | The library (3.6 KB, C, no C library): RomTag, the 30 vectors in the SFD's order, each in its registers. It holds no engine: `JS_NewTask` starts `LIBS:JavaScript/JavaScriptEngine` the first time, then calls the engine's table through its public port; every other call goes straight to the table the task carries. |
| `jsl_core.cpp` | The engine core, on JavaScriptCore's C interface: script tasks, values, calls, host classes (one wrapper per program object, found again through a weak reference; the class's release function is told when the collector drops it), errors with file and line. |
| `jsl_engine.c` | JavaScriptEngine's main: the public port `JAVASCRIPT.ENGINE` with the table; starts and ends script tasks on its own process; quits after `ENV:JavaScriptIdleQuit` seconds (60) with none alive. |
| `jsl_os.h`, `jsl_os_exec.c`, `jsl_os_posix.c` | What the core needs from the system: a script thread (a pthread) and its mailbox (an Exec port on AmigaOS). The POSIX layer is for the tests. |
| `jsl_engine.h`, `jsl_engine_amiga.h` | The table, and the engine's port, shared by the library and the engine. |
| `JS.c` | The Shell command: `print`, `readFile`, `writeFile`, `arguments`. |
| `sfd/javascript_lib.sfd`, `sdk/` | The calls (LVO -30 to -204), and the SDK OpenNDK's `ndkgen.py` makes from them (`scripts/gen-jslib-sdk.sh`); `sdk/Include_H/libraries/javascript.h` by hand. |
| `check_vectors.py` | The library's vector table against the SFD. |

A **script task** is a thread with its own JavaScriptCore VM, heap and
global object. Everything about its scripts runs there, one job at a time.
Any task may call the library: a call from another task is handed to the
script task and waited for; a call from the script task itself (a host
class's function calling back) runs at once. Script tasks run side by
side; the 68k interpreter's register file is swapped at each task switch
(WebKit patch 0015's `tc_Switch`/`tc_Launch` hooks).

Host-class functions run on the script task. They must not use the C
library of the program that registered them (it is another task); Exec and
DOS calls are fine. A slot function returns a new value (`JS_New*`,
`JS_Dup`, `JS_Wrap`) or NULL for undefined, and throws with
`return JS_Throw(task, "message");`.

## Building and testing

- Library and JS: `scripts/build-javascript-library.sh` (os32-gcc16 or the
  OpenAmigaGCC release, OpenNDK). CI: `.github/workflows/jslib.yml`.
- Engine: `scripts/build-javascript-engine.sh` after `scripts/build-jsc.sh`
  (os32-gcc16, the patched WebKit tree).
- Core tests: `tests/jslib/run.sh` builds `tests/jslib/core_test.c` with the
  core against the 68k JIT lab's m68k-linux JavaScriptCore and runs it under
  qemu-m68k (68040): 57 checks of values, calls, errors, host classes,
  wrapper release by the collector, jobs, and three tasks from three
  threads. They also pass with `JSC_slowPathAllocsBetweenGCs=50`.

Install: `javascript.library` in `LIBS:`, `JavaScriptEngine` in
`LIBS:JavaScript/`, `JS` in `C:`.

Next (the design's phases): the translation cache (each script's 68k saved
as a hunk on first call, loaded on the next run), then the host classes
proved on OpenBrowser Lite, then OpenBrowser itself on the library.
