// Virtual calls (patch 0043): a call site that sees more callees than a
// polymorphic stub holds, closures among them, and construct sites whose
// constructors were never called (no call CodeBlock), with arguments,
// results and exceptions through them.
var fs = []; for (var k = 0; k < 20; k++) fs.push(new Function("x", "return x + " + k));
function mega(n) { var s = 0; for (var i = 0; i < n; i++) s += fs[i % 20](i); return s; }
print("mega " + mega(2000));
function maker(k) { return function (x) { return x * k; }; }
var mixed = [Math.max, maker(2), maker(3), function (x) { return -x; }, String, Number, maker(5), (x) => x + 1, Boolean, maker(7), parseInt];
function many(n) { var r = []; for (var i = 0; i < n; i++) r[i % 11] = String(mixed[i % 11](i, 3)); return r.join(","); }
print("many " + many(1100));
function A(v) { this.a = v; } function B(v) { this.b = v; } function C(v) { this.c = v; }
var ctors = [A, B, C, Array, Date, Error, Set];
function cons(n) { var s = ""; for (var i = 0; i < n; i++) { var o = new ctors[i % 7](i % 7 === 6 ? [i] : i); s = Object.prototype.toString.call(o) + (o.a !== undefined ? o.a : "") + (o.b !== undefined ? o.b : "") + (o.c !== undefined ? o.c : ""); } return s; }
print("cons " + cons(701));
function thrower(k) { return function () { if (k === 3) throw new Error("t" + k); return k; }; }
var ts = []; for (var k = 0; k < 12; k++) ts.push(thrower(k));
function throws(n) { var s = 0, c = 0; for (var i = 0; i < n; i++) { try { s += ts[i % 12](); } catch (e) { c++; } } return s + " " + c; }
print("throws " + throws(1200));
