// A small virtual DOM: build trees, diff them, produce patches (React-style).
class VNode {
    constructor(tag, props, children) { this.tag = tag; this.props = props; this.children = children; this.key = props && props.key; }
    get isText() { return this.tag === "#text"; }
}
function h(tag, props) { var kids = []; for (var i = 2; i < arguments.length; i++) { var c = arguments[i]; kids.push(typeof c === "string" ? new VNode("#text", null, c) : c); } return new VNode(tag, props, kids); }
function row(item, sel) { return h("tr", { key: item.id, className: item.id === sel ? "danger" : "" }, h("td", { className: "col-md-1" }, String(item.id)), h("td", { className: "col-md-4" }, h("a", { href: "#" }, item.label)), h("td", { className: "col-md-1" }, h("span", { className: "glyphicon" }))); }
function table(items, sel) { return h("table", { className: "table" }, h("tbody", null, ...items.map(function (it) { return row(it, sel); }))); }
function diffProps(a, b, out) { var k; for (k in b) if (a[k] !== b[k]) out.push({ op: "set", k: k, v: b[k] }); for (k in a) if (!(k in b)) out.push({ op: "del", k: k }); }
function diff(a, b, out) {
    if (a.tag !== b.tag) { out.push({ op: "replace" }); return; }
    if (a.isText) { if (a.children !== b.children) out.push({ op: "text", v: b.children }); return; }
    if (a.props && b.props) diffProps(a.props, b.props, out);
    var n = Math.min(a.children.length, b.children.length);
    for (var i = 0; i < n; i++) diff(a.children[i], b.children[i], out);
    for (; i < b.children.length; i++) out.push({ op: "add" });
    for (; i < a.children.length; i++) out.push({ op: "remove" });
}
var adj = ["pretty", "large", "big", "small", "tall", "short", "long", "handsome", "plain", "quaint"];
var noun = ["table", "chair", "house", "bbq", "desk", "car", "pony", "cookie", "sandwich", "burger"];
run("vdom", function () {
    var items = [], seed = 1, total = 0;
    for (var i = 0; i < 100; i++) { seed = (seed * 1103515245 + 12345) & 0x7fffffff; items.push({ id: i + 1, label: adj[seed % 10] + " " + noun[(seed >> 8) % 10] }); }
    var old = table(items, 0);
    for (var r = 0; r < 12; r++) {
        for (var j = r; j < items.length; j += 10) items[j] = { id: items[j].id, label: items[j].label + " !!!" };
        var cur = table(items, r * 3), out = [];
        diff(old, cur, out); total += out.length; old = cur;
    }
    return total;
});
