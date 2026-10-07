#include "ob_css_lite.h"

#include <stdio.h>

#define CHECK(x) do { if (!(x)) {     printf("CSSLITE FAIL line=%d expr=%s\n", __LINE__, #x); return 20; } } while (0)

int main(void)
{
    static const char css[] =
        "p { color:#123456; font-size:14px; margin:1px 2px; }"
        ".lead { color:red; font-weight:bold; }"
        "p.lead { padding:3px 4px; }"
        "#hero { color:blue; text-align:center; }"
        ".hidden { display:none; }"
        "p span { color:orange; }";
    ob_css_sheet sheet;
    ol_style style;

    ob_css_init(&sheet);
    CHECK(ob_css_add(&sheet, css, sizeof(css) - 1));
    CHECK(ob_css_rule_count(&sheet) == 5);

    ol_style_init(&style);
    style.display = OL_DISPLAY_BLOCK;
    ob_css_apply(&sheet, "p", "hero", "lead extra",
                 "color:#00ff00; font-style:italic; line-height:1.5;",
                 &style);

    CHECK(style.foreground == 0xff00ff00u);
    CHECK(style.font_size == OL_CSSPX(14));
    CHECK(style.line_height == OL_CSSPX(21));
    CHECK(style.text_flags & OL_TEXT_BOLD);
    CHECK(style.text_flags & OL_TEXT_ITALIC);
    CHECK(style.text_align == OL_ALIGN_CENTER);
    CHECK(style.margin_top == OL_CSSPX(1));
    CHECK(style.margin_right == OL_CSSPX(2));
    CHECK(style.margin_bottom == OL_CSSPX(1));
    CHECK(style.margin_left == OL_CSSPX(2));
    CHECK(style.padding_top == OL_CSSPX(3));
    CHECK(style.padding_right == OL_CSSPX(4));

    ol_style_init(&style);
    style.display = OL_DISPLAY_BLOCK;
    ob_css_apply(&sheet, "div", NULL, "hidden", NULL, &style);
    CHECK(style.display == OL_DISPLAY_NONE);

    printf("CSSLITE PASS rules=%lu\n", (unsigned long)ob_css_rule_count(&sheet));
    ob_css_free(&sheet);
    return 0;
}
