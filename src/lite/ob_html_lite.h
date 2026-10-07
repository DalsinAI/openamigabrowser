#ifndef OB_HTML_LITE_H
#define OB_HTML_LITE_H

#include <stddef.h>
#include "openlayout.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    ol_document *document;
    char *title;
    int saw_script;
    int saw_style;
    int unsupported_tags;
} ob_html_lite_result;

int ob_html_lite_parse(const char *html, size_t length,
                       ob_html_lite_result *result);
void ob_html_lite_result_free(ob_html_lite_result *result);

#ifdef __cplusplus
}
#endif
#endif
