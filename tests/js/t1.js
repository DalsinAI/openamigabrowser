print(1+1)
try { print("after") } catch (e) { print("EXC1 " + e) }
try { var x = 6 * 7; print(x) } catch (e) { print("EXC2 " + e) }
try { print([1,2,3].map(function (v) { return v * 2 }).join(",")) } catch (e) { print("EXC3 " + e) }
try { print(JSON.stringify({a: 1, b: "two"})) } catch (e) { print("EXC4 " + e) }
try { print(Math.sqrt(2)) } catch (e) { print("EXC5 " + e) }
print("end")
