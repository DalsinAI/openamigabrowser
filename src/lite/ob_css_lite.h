#ifndef OB_CSS_LITE_H
#define OB_CSS_LITE_H

#include <stddef.h>
#include "ob_openlayout_api.h"

typedef struct ob_css_rule ob_css_rule;

typedef struct {
    ob_css_rule *rules;
    size_t count;
    size_t capacity;
    unsigned order;
} ob_css_sheet;

void ob_css_init(ob_css_sheet *sheet);
void ob_css_free(ob_css_sheet *sheet);
int ob_css_add(ob_css_sheet *sheet, const char *css, size_t length);
void ob_css_apply(const ob_css_sheet *sheet,
                  const char *tag, const char *id, const char *classes,
                  const char *inline_style, ol_style *style);
size_t ob_css_rule_count(const ob_css_sheet *sheet);

#endif
