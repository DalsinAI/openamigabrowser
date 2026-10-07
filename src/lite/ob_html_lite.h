#ifndef OB_HTML_LITE_H
#define OB_HTML_LITE_H

#include <stddef.h>
#include "ob_openlayout_api.h"

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
} ob_html_lite_result;

int ob_html_lite_parse(const char *html, size_t length,
                       ob_html_lite_result *result);
int ob_html_lite_parse_with_css(const char *html, size_t length,
                                const char *extra_css, size_t extra_css_length,
                                ob_html_lite_result *result);
void ob_html_lite_result_free(ob_html_lite_result *result);

#ifdef __cplusplus
}
#endif
#endif
