// a polymorphic closure call site, then a callee that is not a function
function mk(k) { return function (x) { return x + k; }; }
function callIt(f, x) { return f(x); }
function cc(s) { return s.charCodeAt(0); }
var fs = [mk(1), mk(2), mk(3), mk(4)], s = 0, errs = 0;
for (var i = 0; i < 4000; i++) s += callIt(fs[i & 3], i);
var bad = [null, undefined, true, false, 5, "str", {}];
for (var j = 0; j < bad.length; j++) { try { callIt(bad[j], 1); } catch (e) { errs++; } }
for (var i = 0; i < 2000; i++) s += cc("abc");
var o = { charCodeAt: null };
try { cc(o); } catch (e) { errs++; }
for (var i = 0; i < 100; i++) s += callIt(fs[i & 3], i);
print("callees", s, errs);
