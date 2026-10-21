# OB_OPTIMIZE=layout: the engine is built for size (-Os) except WebCore's
# style, layout and DOM code (css/, dom/, layout/, rendering/, style/),
# which gets -O2. -O2 throughout ran the DOM bench about 25 % faster
# (layout-reads is most of it) but made the engine 26 MB larger and page
# loads 0.3-1 s slower; JavaScriptCore alone at -O2 gained nothing.
# WebCore is built from unified sources (each bundle includes several .cpp
# files of one folder), so the bundles are read to find their folder.
# Read through CMAKE_PROJECT_INCLUDE, so after each project(); WebCore
# exists only at the end of the top-level folder, hence the deferred call.
get_property(_ob_optimize_layout_done GLOBAL PROPERTY OB_OPTIMIZE_LAYOUT_DEFERRED)
if (NOT _ob_optimize_layout_done)
    set_property(GLOBAL PROPERTY OB_OPTIMIZE_LAYOUT_DEFERRED TRUE)
    function(_ob_optimize_layout)
        if (NOT TARGET WebCore)
            message(WARNING "OB_OPTIMIZE=layout: no target WebCore")
            return()
        endif ()
        set(folders "(css|dom|layout|rendering|style)/")
        get_target_property(sources WebCore SOURCES)
        get_target_property(sourceDir WebCore SOURCE_DIR)
        set(count 0)
        foreach (source IN LISTS sources)
            set(fast FALSE)
            # Relative names are WebCore's; here they would be read as the
            # top-level folder's.
            if (IS_ABSOLUTE "${source}")
                set(path "${source}")
            else ()
                set(path "${sourceDir}/${source}")
            endif ()
            if (source MATCHES "UnifiedSource[^/]*\\.cpp$")
                if (EXISTS "${path}")
                    file(STRINGS "${path}" includes REGEX "^#include \"${folders}")
                    if (includes)
                        set(fast TRUE)
                    endif ()
                endif ()
            elseif (source MATCHES "(^|/)${folders}[^/]*\\.cpp$")
                set(fast TRUE)
            endif ()
            if (fast)
                # After CMAKE_CXX_FLAGS_MINSIZEREL's -Os on the command line,
                # so this -O2 is the one GCC keeps.
                set_source_files_properties("${path}" TARGET_DIRECTORY WebCore PROPERTIES COMPILE_OPTIONS -O2)
                math(EXPR count "${count} + 1")
            endif ()
        endforeach ()
        message(STATUS "OB_OPTIMIZE=layout: ${count} WebCore sources at -O2")
    endfunction()
    cmake_language(DEFER DIRECTORY "${CMAKE_SOURCE_DIR}" CALL _ob_optimize_layout)
endif ()
