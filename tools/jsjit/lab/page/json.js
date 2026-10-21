// An API response: JSON.parse, walk, filter/map/reduce, JSON.stringify.
var src = [];
for (var i = 0; i < 80; i++) src.push({ id: i, name: "Product " + i, price: (i * 37 % 100) + 0.99, tags: ["a" + (i % 5), "b" + (i % 3)], stock: { warehouse: i % 4, count: i * 3 % 17 }, active: i % 3 !== 0 });
var payload = JSON.stringify({ status: "ok", page: 1, results: src });
run("json", function () {
    var acc = 0;
    for (var r = 0; r < 30; r++) {
        var data = JSON.parse(payload);
        var live = data.results.filter(function (p) { return p.active && p.stock.count > 2; });
        var names = live.map(function (p) { return p.name.toLowerCase(); });
        var value = live.reduce(function (s, p) { return s + p.price * p.stock.count; }, 0);
        var byTag = {};
        live.forEach(function (p) { p.tags.forEach(function (t) { byTag[t] = (byTag[t] || 0) + 1; }); });
        acc += names.length + Math.round(value) + Object.keys(byTag).length + JSON.stringify(live.slice(0, 10)).length;
    }
    return acc;
});
