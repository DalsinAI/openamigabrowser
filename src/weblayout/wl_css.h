#ifndef WL_CSS_H
#define WL_CSS_H

#include <stddef.h>
#include "wl_openlayout.h"

typedef struct wl_css_rule wl_css_rule;

typedef struct {
    wl_css_rule *rules;
    size_t count;
    size_t capacity;
    unsigned order;
} wl_css_sheet;

void wl_css_init(wl_css_sheet *sheet);
void wl_css_free(wl_css_sheet *sheet);
int wl_css_add(wl_css_sheet *sheet, const char *css, size_t length);
void wl_css_apply(const wl_css_sheet *sheet,
                  const char *tag, const char *id, const char *classes,
                  const char *inline_style, ol_style *style);
size_t wl_css_rule_count(const wl_css_sheet *sheet);

#endif
