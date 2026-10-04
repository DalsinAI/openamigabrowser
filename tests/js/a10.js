var o = {a: 1, b: "two", c: [1,2]}
print(JSON.stringify(o))
function fib(n) { return n < 2 ? n : fib(n-1) + fib(n-2) }
print(fib(15))
let s = 0; for (let i = 0; i < 1000; i++) s += i; print(s)
print([3,1,2].sort().join("-"))
print("abc".toUpperCase() + String(42))
try { null.x } catch (e) { print("caught " + e.name) }
print(new Date(0).toISOString())
print(/a(b+)c/.exec("xabbbc")[1])
