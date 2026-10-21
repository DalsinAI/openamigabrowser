// `in` by value: arrays of int32s, of values and of doubles, holes,
// elements on the prototype, out-of-range and negative indexes, non-int32
// keys, objects, strings (which throw), proxies, and arrays that change
// shape, in loops and in straight code.
function count(a, n) { var t = 0; for (var i = -2; i < n; i++) if (i in a) t++; return t; }
var ints = [1, 2, 3, 4, 5], vals = [1, "a", {}, null, undefined], dbls = [1.5, 2.5, NaN];
var holes = [1, , 3, , 5]; holes.length = 9;
print("ints " + count(ints, 8) + " vals " + count(vals, 8) + " dbls " + count(dbls, 8) + " holes " + count(holes, 12));
Array.prototype[3] = "proto";
print("proto " + count(holes, 12) + " " + count([0, , , , ], 6));
delete Array.prototype[3];
print("unproto " + count(holes, 12));
function keys(a) { var r = []; var ks = [0, 1, 1.5, "1", "length", "x", -0, 2 ** 31, -1, true]; for (var j = 0; j < 300; j++) { r = []; for (var k of ks) r.push(k in a); } return r.join(","); }
print("keys " + keys([5, 6, 7]));
var o = { 0: "a", 2: "b", x: 1 };
print("object " + count(o, 5));
var shapes = [[1, 2], ["a", "b"], [1.5], [, 1], {length: 2, 0: 1}, new Proxy([1, 2, 3], { has(t, k) { return k === "1"; } })];
var out = []; for (var r = 0; r < 200; r++) { out = []; for (var s of shapes) out.push(count(s, 4)); }
print("shapes " + out.join(","));
var grow = []; var g = 0; for (var i = 0; i < 500; i++) { grow.push(i); if ((i >> 1) in grow) g++; if (i === 250) grow[1000] = 1; }
print("grow " + g + " " + (999 in grow) + " " + (1000 in grow));
try { for (var i = 0; i < 300; i++) (0 in (i < 299 ? [1] : "str")); } catch (e) { print("string " + e.constructor.name); }
function builtins(n) { var a = []; for (var i = 0; i < n; i++) a.push(i % 7); delete a[3]; var f = a.filter(x => x > 2), m = a.map(x => x * 2), s = 0; a.forEach(x => { s += x; }); return f.length + " " + m.length + " " + s + " " + a.indexOf(6) + " " + a.includes(undefined) + " " + a.reduce((p, x) => p + x, 0); }
print("builtins " + builtins(400));
// arguments objects: length and elements, fewer and more arguments than
// parameters, out of range, overridden length, deleted and redefined
// elements, arguments written through parameters (sloppy mode), strict
// mode (ScopedArguments-free), and a function that leaks its arguments.
function sum() { var t = 0; for (var i = 0; i < arguments.length; i++) t += arguments[i]; return t; }
function args(n) { var t = 0; for (var i = 0; i < n; i++) t += sum(i, 1, 2) + sum() + sum(i); return t; }
print("args " + args(600));
function edge(a, b) { var r = [arguments.length, arguments[0], arguments[1], arguments[2], arguments[-1], arguments[1.5]]; return r.join("/"); }
var e = ""; for (var i = 0; i < 300; i++) e = edge(i) + " " + edge(1, 2, 3); print("edge " + e);
function over() { arguments.length = 1; return arguments.length + ":" + arguments[1]; }
function del() { delete arguments[0]; return arguments.length + ":" + arguments[0] + ":" + arguments[1]; }
function redef() { Object.defineProperty(arguments, 0, { get() { return "g"; } }); return arguments[0]; }
function alias(a) { a = 9; return arguments[0] + arguments.length; }
function strict(a) { "use strict"; a = 9; return arguments[0] + arguments.length; }
var z = ""; for (var i = 0; i < 300; i++) z = [over(1, 2), del(1, 2), redef(1), alias(1), strict(1)].join(" "); print("special " + z);
function leak() { return arguments; }
var lk = 0; for (var i = 0; i < 300; i++) { var la = leak(i, i + 1); lk += la.length + la[1]; } print("leak " + lk);
// for-in: small objects of one shape and of many, empty objects, indexed
// properties, prototype properties, properties added and deleted during
// the loop, nested loops, arrays, strings, null and undefined, proxies,
// dictionaries, and the name read back with o[k] and k in o.
function keysOf(o) { var r = ""; for (var k in o) r += k + "=" + o[k] + (typeof o !== "object" || k in o ? ";" : "!;"); return r; }
function many(n) { var t = 0, last = ""; for (var i = 0; i < n; i++) { last = keysOf({ a: i, b: 2 }) + keysOf({}) + keysOf({ x: 1, y: 2, z: 3, w: i & 1 }); t += last.length; } return t + " " + last; }
print("forin " + many(400));
function Base() { this.own = 1; } Base.prototype.inherited = 2;
var ix = { 1: "one", 0: "zero", s: "s" }, arr = [7, , 9]; arr.extra = "e";
var dict = {}; for (var i = 0; i < 70; i++) dict["p" + i] = i; for (var i = 0; i < 60; i++) delete dict["p" + i];
var pr = new Proxy({ q: 1, r: 2 }, { ownKeys(t) { return ["r", "q"]; } });
var shapes = [new Base(), ix, arr, "str", null, undefined, 42, dict, pr, Object.create(null)];
var res = []; for (var r = 0; r < 100; r++) { res = []; for (var s of shapes) res.push(keysOf(s)); } print("kinds " + res.join(" | "));
function mutate(n) { var out = ""; for (var j = 0; j < n; j++) { var o = { a: 1, b: 2, c: 3 }; out = ""; for (var k in o) { out += k; if (k === "a") { delete o.b; o.d = 4; } } } return out; }
print("mutate " + mutate(300));
function nested(n) { var c = 0; for (var j = 0; j < n; j++) { var o = { a: 1, b: { x: 1, y: 2 } }; for (var k in o) for (var m in o.b) c += k.length + m.length; } return c; }
print("nestedForIn " + nested(300));
function changeShape(n) { var o = { a: 1, b: 2 }, s = ""; for (var j = 0; j < n; j++) { s = ""; for (var k in o) { s += k; if (j === 150) o.c = 3; } } return s; }
print("changeShape " + changeShape(300));
// === and !== on strings: literals (atoms), strings built at run time
// (not atoms) with the same characters, ropes, empty strings, numbers
// turned into strings, keys from for-in, against constants, BigInts and
// symbols.
function seq(a, b) { return (a === b ? "=" : "") + (a !== b ? "!" : ""); }
function strs(n) {
    var built = ["ab", "a" + "b".repeat(1), ["a", "b"].join(""), "abc".slice(0, 2), "", String(12), 12 + "", "x"];
    var out = "";
    for (var r = 0; r < n; r++) {
        out = "";
        for (var i = 0; i < built.length; i++) {
            for (var j = 0; j < built.length; j++) out += seq(built[i], built[j]);
            out += (built[i] === "ab" ? "A" : "a") + (built[i] !== "12" ? "t" : "T") + (built[i] === "" ? "E" : "e") + "|";
        }
        var rope = "a"; rope += "b"; out += seq(rope, "ab") + seq(rope + "c", "abc") + seq(10n, "10") + seq(10n, 10n) + seq(Symbol.iterator, Symbol.iterator) + seq("s", Symbol("s"));
        for (var k in { ab: 1, cd: 2 }) out += seq(k, "ab") + seq(k, built[0]);
    }
    return out;
}
print("strs " + strs(300));
