// Template rendering: build HTML strings with escaping (blog/comment lists).
var ESC = { "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;" };
function escapeHtml(s) { return String(s).replace(/[&<>"']/g, function (c) { return ESC[c]; }); }
function renderComment(c) {
    return `<li class="comment${c.score > 5 ? " hot" : ""}" id="c${c.id}"><b>${escapeHtml(c.user)}</b> <span>${c.score} points</span><p>${escapeHtml(c.text)}</p></li>`;
}
run("template", function () {
    var comments = [];
    for (var i = 0; i < 60; i++) comments.push({ id: i, user: "user" + (i % 7) + (i % 3 ? "" : "<admin>"), score: (i * 7) % 11, text: "This is comment number " + i + " & it says \"hello\" to <everyone>." });
    var len = 0;
    for (var r = 0; r < 40; r++) {
        var html = "<ul>";
        for (var j = 0; j < comments.length; j++) html += renderComment(comments[j]);
        html += "</ul>";
        var parts = []; comments.forEach(function (c) { parts.push(c.user.toUpperCase()); });
        len += html.length + parts.join(",").length;
    }
    return len;
});
