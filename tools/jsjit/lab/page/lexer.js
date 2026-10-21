// A hand-written tokenizer (what minifiers, markdown and syntax highlighters do).
var code = "function add(a, b) { return a + b * 2; } // sum\nvar x = add(10, 20); if (x >= 50) { x = x - 1; } else { x += \"str\"; }\n";
var text = ""; for (var i = 0; i < 6; i++) text += code;
function isIdent(c) { return (c >= 97 && c <= 122) || (c >= 65 && c <= 90) || c === 95 || c === 36 || (c >= 48 && c <= 57); }
var KEYWORDS = { "function": 1, "return": 1, "var": 1, "if": 1, "else": 1 };
function lex(s) {
    var toks = [], i = 0, n = s.length;
    while (i < n) {
        var c = s.charCodeAt(i);
        if (c === 32 || c === 10 || c === 9) { i++; continue; }
        if (c === 47 && s.charCodeAt(i + 1) === 47) { while (i < n && s.charCodeAt(i) !== 10) i++; continue; }
        var start = i;
        if (isIdent(c) && !(c >= 48 && c <= 57)) { while (i < n && isIdent(s.charCodeAt(i))) i++; var w = s.slice(start, i); toks.push({ t: KEYWORDS[w] ? "kw" : "id", v: w }); continue; }
        if (c >= 48 && c <= 57) { var v = 0; while (i < n && (c = s.charCodeAt(i)) >= 48 && c <= 57) { v = v * 10 + c - 48; i++; } toks.push({ t: "num", v: v }); continue; }
        if (c === 34) { i++; while (i < n && s.charCodeAt(i) !== 34) i++; i++; toks.push({ t: "str", v: s.slice(start + 1, i - 1) }); continue; }
        switch (s[i]) { case ">": case "<": case "=": case "+": case "-": if (s[i + 1] === "=") { toks.push({ t: "op", v: s.substr(i, 2) }); i += 2; continue; } }
        toks.push({ t: "punc", v: s[i] }); i++;
    }
    return toks;
}
run("lexer", function () { var n = 0; for (var r = 0; r < 30; r++) n += lex(text).length; return n; });
