# OB_OPTIMIZE=js: the engine is built for size (-Os) except JavaScriptCore
# and the interpreter beside it, which get -O2. A whole -O2 build ran the
# DOM bench about 25 % faster but made the engine 26 MB larger and every
# page load 0.3-1 s slower, so only the JavaScript engine is built for speed.
# WebKit patch 0059 builds JavaScriptCore's cold parts (Intl, Temporal,
# the typed array prototype functions, the C API, the debugger and the
# like) back at -Os, per source, after this -O2.
# Read through CMAKE_PROJECT_INCLUDE, so after each project(); the targets
# exist only at the end of the top-level folder, hence the deferred call.
get_property(_ob_optimize_js_done GLOBAL PROPERTY OB_OPTIMIZE_JS_DEFERRED)
if (NOT _ob_optimize_js_done)
    set_property(GLOBAL PROPERTY OB_OPTIMIZE_JS_DEFERRED TRUE)
    function(_ob_optimize_js)
        foreach (target JavaScriptCore LowLevelInterpreterLib)
            if (TARGET ${target})
                # After CMAKE_CXX_FLAGS_MINSIZEREL's -Os on the command line,
                # so this -O2 is the one GCC keeps.
                target_compile_options(${target} PRIVATE -O2)
            else ()
                message(WARNING "OB_OPTIMIZE=js: no target ${target}")
            endif ()
        endforeach ()
    endfunction()
    cmake_language(DEFER DIRECTORY "${CMAKE_SOURCE_DIR}" CALL _ob_optimize_js)
endif ()
