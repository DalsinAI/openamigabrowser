#ifndef WL_HTML_H
#define WL_HTML_H

#include <stddef.h>
#include "wl_openlayout.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    ol_document *document;
    char *title;
    char **stylesheet_href;
    int nstylesheets;
    int saw_script;
    int saw_style;
    int unsupported_tags;
} wl_html_result;

int wl_html_parse(const char *html, size_t length,
                       wl_html_result *result);
int wl_html_parse_with_css(const char *html, size_t length,
                                const char *extra_css, size_t extra_css_length,
                                wl_html_result *result);
void wl_html_result_free(wl_html_result *result);

#ifdef __cplusplus
}
#endif
#endif
