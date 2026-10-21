// Objects made in line (patch 0044): {} literals small and large, constructors and
// classes, a constructor whose prototype changes, Reflect.construct with
// another new.target, constructors that return objects, functions both
// called and constructed, classes made by a factory (poly proto), many
// objects across garbage collections, and objects kept and read back.
function lits(n) { var keep = [], t = 0; for (var i = 0; i < n; i++) { var a = {}, b = { x: i }, c = { a: 1, b: 2, c: 3, d: 4, e: 5, f: 6, g: 7, h: 8 }; a.y = i; c.i = 9; t += a.y + b.x + c.i + c.h; if (i % 50 === 0) keep.push(a, b, c); } return t + " " + keep.length + " " + JSON.stringify(keep[7]) + JSON.stringify(keep[8]); }
print("lits " + lits(2000));
function Point(x, y) { this.x = x; this.y = y; }
Point.prototype.len = function () { return this.x * this.x + this.y * this.y; };
function points(n) { var t = 0, last; for (var i = 0; i < n; i++) { last = new Point(i, 2); t += last.len(); if (i === n >> 1) Point.prototype = { len: function () { return -1; } }; } return t + " " + (last instanceof Point) + " " + Object.keys(last); }
print("points " + points(1000));
class Animal { constructor(n) { this.n = n; } speak() { return this.n; } }
class Dog extends Animal { constructor(n) { super(n); this.d = 1; } }
function classes(n) { var s = 0; for (var i = 0; i < n; i++) { s += new Animal(i).speak() + new Dog(i).d; } return s; }
print("classes " + classes(800));
function Other() { this.o = 1; }
function nt(n) { var r = ""; for (var i = 0; i < n; i++) { var p = Reflect.construct(Point, [1, 2], i & 1 ? Other : Point); r = Object.getPrototypeOf(p) === Other.prototype ? "other" : "point"; } return r + " " + JSON.stringify(Reflect.construct(Point, [3, 4], Other)); }
print("newTarget " + nt(301));
function Ret(k) { this.a = 1; if (k) return { ret: k }; }
function rets(n) { var s = 0; for (var i = 0; i < n; i++) { var o = new Ret(i & 1); s += o.a === 1 ? 1 : o.ret * 10; } return s; }
print("rets " + rets(500));
function Both(x) { if (!new.target) return "called " + x; this.x = x; }
function both(n) { var r = ""; for (var i = 0; i < n; i++) r = Both(i) + " " + new Both(i).x; return r; }
print("both " + both(400));
function factory() { return class { constructor(v) { this.v = v; } get() { return this.v; } }; }
function poly(n) { var cs = [factory(), factory(), factory()], s = 0; for (var i = 0; i < n; i++) { var o = new cs[i % 3](i); s += o.get() + (o instanceof cs[i % 3] ? 1 : 0); } return s; }
print("poly " + poly(900));
function churn(n) { var held = [], s = 0; for (var i = 0; i < n; i++) { var o = { i: i, p: new Point(i, i) }; if (i % 97 === 0) held.push(o); if (i % 1000 === 0 && typeof gc === "function") gc(); s += o.p.x; } var c = 0; for (var h of held) c += h.i + h.p.y; return s + " " + c; }
print("churn " + churn(20000));
function big(n) { var o; for (var i = 0; i < n; i++) { o = {}; for (var k = 0; k < 20; k++) o["k" + k] = k * i; } return JSON.stringify(o).length; }
print("big " + big(200));
