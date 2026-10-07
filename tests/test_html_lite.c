#include "ob_html_lite.h"

#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) {     printf("HTMLLITE FAIL line=%d expr=%s\n", __LINE__, #x); return 20; } } while (0)

static size_t count_type(const ol_document *doc, ol_op_type type)
{
    size_t i, n = 0;
    for (i = 0; i < ol_display_count(doc); ++i) {
        const ol_display_op *op = ol_display_get(doc, i);
        if (op && op->type == type) ++n;
    }
    return n;
}

int main(void)
{
    static const char html[] =
        "<!doctype html><html><head>"
        "<title>OpenBrowser &amp; OpenLayout</title>"
        "<style>body{color:red}</style></head><body>"
        "<h1>First light</h1>"
        "<p>A tiny browser can keep <strong>semantics</strong> and "
        "<a href=\"https://example.com/\">real links</a> without WebCore.</p>"
        "<ul><li>Small</li><li>Fast</li></ul>"
        "<p><img src=\"logo.png\" alt=\"Logo\" width=\"80\" height=\"40\"> image.</p>"
        "<script>document.body.textContent='not executed';</script>"
        "</body></html>";
    ob_html_lite_result r;
    const ol_node *link;
    ol_rect box;
    ol_unit h1, h2;

    CHECK(ob_html_lite_parse(html, sizeof(html) - 1, &r));
    CHECK(r.document != NULL);
    CHECK(r.title != NULL);
    CHECK(strcmp(r.title, "OpenBrowser & OpenLayout") == 0);
    CHECK(r.saw_script == 1);
    CHECK(r.saw_style == 1);
    CHECK(r.unsupported_tags == 0);

    CHECK(ol_layout(r.document, OL_CSSPX(360), NULL, NULL));
    h1 = ol_document_content_height(r.document);
    CHECK(h1 > 0);
    CHECK(count_type(r.document, OL_OP_TEXT) >= 8);
    CHECK(count_type(r.document, OL_OP_IMAGE) == 1);

    link = ol_find_role_name(r.document, OL_ROLE_LINK, "https://example.com/");
    CHECK(link != NULL);
    CHECK(strcmp(ol_node_href(link), "https://example.com/") == 0);
    CHECK(ol_node_actions(link) & OL_ACTION_ACTIVATE);
    box = ol_node_bounds(link);
    CHECK(box.width > 0 && box.height > 0);
    CHECK(ol_node_id(ol_hit_action(r.document, box.x + 1, box.y + 1,
                                OL_ACTION_ACTIVATE)) == ol_node_id(link));

    CHECK(ol_layout(r.document, OL_CSSPX(180), NULL, NULL));
    h2 = ol_document_content_height(r.document);
    CHECK(h2 > h1);
    CHECK(ol_node_id(ol_find_role_name(r.document, OL_ROLE_LINK,
          "https://example.com/")) == ol_node_id(link));

    printf("HTMLLITE PASS title=\"%s\" wide_px=%ld narrow_px=%ld ops=%lu script=%d\n",
           r.title,
           (long)(h1 / OL_UNITS_PER_CSSPX),
           (long)(h2 / OL_UNITS_PER_CSSPX),
           (unsigned long)ol_display_count(r.document),
           r.saw_script);

    ob_html_lite_result_free(&r);
    return 0;
}
