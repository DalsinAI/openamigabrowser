# Real-page JavaScript benchmark

Five tests that do what web pages and their frameworks do, without a DOM:
`vdom.js` (build and diff virtual DOM trees, React style), `template.js`
(render HTML strings with escaping), `json.js` (parse, walk and rebuild an
API response), `lexer.js` (a hand-written tokenizer), `events.js` (an event
emitter and a Redux-style store). Each prints `JSBENCH page-<name> <ms>
(<result>)`; the result must match node's.

    cat hdr.js vdom.js > /tmp/p_vdom.js
    ../count.sh path/to/jsc /tmp/p_vdom.js on

Run it with JIT on and off; take 6.3 M (jsc start-up) off the counts.
