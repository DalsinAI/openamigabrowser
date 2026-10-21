// An app model: module pattern, event emitter, store with listeners (jQuery/Redux style).
var Store = (function () {
    var state = { count: 0, todos: [] }, listeners = [], dispatches = 0;
    function notify() { for (var i = 0; i < listeners.length; i++) listeners[i](state); }
    return {
        subscribe: function (f) { listeners.push(f); return function () { listeners = listeners.filter(function (g) { return g !== f; }); }; },
        dispatch: function (action) {
            dispatches++;
            switch (action.type) {
            case "inc": state = Object.assign({}, state, { count: state.count + action.by }); break;
            case "add": state = Object.assign({}, state, { todos: state.todos.concat([{ text: action.text, done: false }]) }); break;
            case "toggle": state = Object.assign({}, state, { todos: state.todos.map(function (t, i) { return i === action.i ? { text: t.text, done: !t.done } : t; }) }); break;
            }
            notify();
        },
        get dispatches() { return dispatches; }
    };
})();
function Emitter() { this.handlers = {}; }
Emitter.prototype.on = function (ev, f) { (this.handlers[ev] || (this.handlers[ev] = [])).push(f); return this; };
Emitter.prototype.emit = function (ev, arg) { var hs = this.handlers[ev]; if (hs) for (var i = 0; i < hs.length; i++) hs[i].call(this, arg); return this; };
run("events", function () {
    var seen = 0, done = 0, bus = new Emitter();
    var unsub = Store.subscribe(function (s) { seen += s.count & 1; });
    Store.subscribe(function (s) { done = s.todos.filter(function (t) { return t.done; }).length; });
    bus.on("click", function (e) { Store.dispatch({ type: "inc", by: e.x }); }).on("key", function (e) { Store.dispatch({ type: "add", text: "todo " + e.k }); });
    for (var i = 0; i < 1500; i++) {
        bus.emit("click", { x: i & 3 });
        if (i % 10 === 0) bus.emit("key", { k: i });
        if (i % 25 === 0) Store.dispatch({ type: "toggle", i: (i / 25) | 0 });
    }
    unsub();
    return seen + done + Store.dispatches;
});
