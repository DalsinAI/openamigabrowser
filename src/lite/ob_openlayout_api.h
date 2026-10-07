#ifndef OB_OPENLAYOUT_API_H
#define OB_OPENLAYOUT_API_H

#include "openlayout.h"

#ifdef OB_USE_OPENLAYOUT_LIBRARY
#include <proto/openlayout.h>

int ob_openlayout_open(void);
void ob_openlayout_close(void);

static inline ol_document *obol_document_new(void)
{
    return OL_DocumentNew();
}

static inline void obol_document_free(ol_document *doc)
{
    OL_DocumentFree(doc);
}

static inline ol_node *obol_document_root(ol_document *doc)
{
    return OL_DocumentRoot(doc);
}

static inline void obol_style_init(ol_style *style)
{
    OL_StyleInit(style);
}

static inline ol_node *obol_node_append(ol_document *doc, ol_node *parent,
                                        ol_role role, const char *text)
{
    struct OLNodeAppend r;
    r.document = doc;
    r.parent = parent;
    r.role = (ULONG)role;
    r.text = (CONST_STRPTR)text;
    return OL_NodeAppend(&r);
}

static inline ol_id obol_node_id(const ol_node *node)
{
    return (ol_id)OL_NodeID(node);
}

static inline ol_role obol_node_role(const ol_node *node)
{
    return (ol_role)OL_NodeRole(node);
}

static inline const char *obol_node_text(const ol_node *node)
{
    return (const char *)OL_NodeText(node);
}

static inline const char *obol_node_name(const ol_node *node)
{
    return (const char *)OL_NodeName(node);
}

static inline const char *obol_node_value(const ol_node *node)
{
    return (const char *)OL_NodeValue(node);
}

static inline const char *obol_node_href(const ol_node *node)
{
    return (const char *)OL_NodeHref(node);
}

static inline const ol_node *obol_node_by_id(const ol_document *doc, ol_id id)
{
    return OL_NodeByID(doc, (ULONG)id);
}

static inline ol_rect obol_node_bounds(const ol_node *node)
{
    struct OLNodeBounds r;
    ol_rect bounds;
    bounds.x = bounds.y = bounds.width = bounds.height = 0;
    r.node = node;
    r.bounds = &bounds;
    if (!OL_NodeBounds(&r)) {
        bounds.x = bounds.y = bounds.width = bounds.height = 0;
    }
    return bounds;
}

static inline int obol_node_set_text(ol_node *node, const char *text)
{
    struct OLNodeString r;
    r.node = node;
    r.text = (CONST_STRPTR)text;
    return OL_NodeSetText(&r) ? 1 : 0;
}

static inline int obol_node_set_name(ol_node *node, const char *text)
{
    struct OLNodeString r;
    r.node = node;
    r.text = (CONST_STRPTR)text;
    return OL_NodeSetName(&r) ? 1 : 0;
}

static inline int obol_node_set_value(ol_node *node, const char *text)
{
    struct OLNodeString r;
    r.node = node;
    r.text = (CONST_STRPTR)text;
    return OL_NodeSetValue(&r) ? 1 : 0;
}

static inline int obol_node_set_href(ol_node *node, const char *text)
{
    struct OLNodeString r;
    r.node = node;
    r.text = (CONST_STRPTR)text;
    return OL_NodeSetHref(&r) ? 1 : 0;
}

static inline void obol_node_set_style(ol_node *node, const ol_style *style)
{
    struct OLNodeStyle r;
    r.node = node;
    r.style = style;
    OL_NodeSetStyle(&r);
}

static inline void obol_node_set_actions(ol_node *node, uint32_t actions)
{
    OL_NodeSetActions(node, (ULONG)actions);
}

static inline uint32_t obol_node_actions(const ol_node *node)
{
    return (uint32_t)OL_NodeActions(node);
}

static inline void obol_node_set_image(ol_node *node, int image_id,
                                        ol_unit width, ol_unit height)
{
    struct OLNodeImage r;
    r.node = node;
    r.image_id = (LONG)image_id;
    r.width = width;
    r.height = height;
    OL_NodeSetImage(&r);
}

static inline const ol_node *obol_find_role_name(const ol_document *doc,
                                                  ol_role role,
                                                  const char *name)
{
    struct OLFindRoleName r;
    r.document = doc;
    r.role = (ULONG)role;
    r.name = (CONST_STRPTR)name;
    return OL_FindRoleName(&r);
}

static inline const ol_node *obol_hit_action(const ol_document *doc,
                                              ol_unit x, ol_unit y,
                                              uint32_t action)
{
    struct OLHitAction r;
    r.document = doc;
    r.x = x;
    r.y = y;
    r.action = (ULONG)action;
    return OL_HitAction(&r);
}

static inline int obol_layout(ol_document *doc, ol_unit width,
                              ol_measure_text_fn measure, void *userdata)
{
    struct OLLayoutRequest r;
    r.document = doc;
    r.viewport_width = width;
    r.measure = measure;
    r.measure_userdata = (APTR)userdata;
    return OL_Layout(&r) ? 1 : 0;
}

static inline ol_unit obol_document_content_height(const ol_document *doc)
{
    return (ol_unit)OL_DocumentContentHeight(doc);
}

static inline uint32_t obol_document_generation(const ol_document *doc)
{
    return (uint32_t)OL_DocumentGeneration(doc);
}

static inline size_t obol_display_count(const ol_document *doc)
{
    return (size_t)OL_DisplayCount(doc);
}

static inline const ol_display_op *obol_display_get(const ol_document *doc,
                                                     size_t index)
{
    return OL_DisplayGet(doc, (ULONG)index);
}

#define ol_document_new obol_document_new
#define ol_document_free obol_document_free
#define ol_document_root obol_document_root
#define ol_style_init obol_style_init
#define ol_node_append obol_node_append
#define ol_node_id obol_node_id
#define ol_node_role obol_node_role
#define ol_node_text obol_node_text
#define ol_node_name obol_node_name
#define ol_node_value obol_node_value
#define ol_node_href obol_node_href
#define ol_node_by_id obol_node_by_id
#define ol_node_bounds obol_node_bounds
#define ol_node_set_text obol_node_set_text
#define ol_node_set_name obol_node_set_name
#define ol_node_set_value obol_node_set_value
#define ol_node_set_href obol_node_set_href
#define ol_node_set_style obol_node_set_style
#define ol_node_set_actions obol_node_set_actions
#define ol_node_actions obol_node_actions
#define ol_node_set_image obol_node_set_image
#define ol_find_role_name obol_find_role_name
#define ol_hit_action obol_hit_action
#define ol_layout obol_layout
#define ol_document_content_height obol_document_content_height
#define ol_document_generation obol_document_generation
#define ol_display_count obol_display_count
#define ol_display_get obol_display_get

#else

static inline int ob_openlayout_open(void) { return 1; }
static inline void ob_openlayout_close(void) { }

#endif

#endif
