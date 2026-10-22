/*
 * javascript.library engine core tests, on Linux under qemu-m68k
 * (tests/jslib/run.sh), through the engine's table as the library calls it.
 * Each check prints "ok" or "FAIL"; the exit code is the number of failures.
 * MIT, Copyright (c) 2026 AmigaChrome Team.
 */
#include <pthread.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "jsl_engine.h"

static const struct JSEngineTable *E;
static int failures;

static void check(int good, const char *what)
{
    printf("%s %s\n", good ? "ok  " : "FAIL", what);
    if (!good)
        failures++;
}

/* The string a script makes, as text */
static void evalText(JSTask t, const char *src, char *out, long size)
{
    JSValue v = NULL;
    out[0] = 0;
    if (E->Evaluate(t, (CONST_STRPTR)src, -1, (CONST_STRPTR)"test.js", &v) != JSERR_OK) {
        E->ErrorText(t, (STRPTR)out, size);
        E->Free(t, v);
        return;
    }
    E->ToString(t, v, (STRPTR)out, size);
    E->Free(t, v);
}

static int evalIs(JSTask t, const char *src, const char *want)
{
    char text[256];
    evalText(t, src, text, sizeof text);
    if (strcmp(text, want)) {
        printf("     %s\n     gave \"%s\", want \"%s\"\n", src, text, want);
        return 0;
    }
    return 1;
}

/* ---- A host class: a point the program owns -------------------------------- */

struct Point {
    LONG x, y;
    UBYTE visible;
    const char *label;
    int released;
};

static JSValue pointLength2(JSTask t, APTR self, LONG argc, JSValue *argv, APTR data)
{
    struct Point *p = self;
    (void)argc; (void)argv; (void)data;
    return E->NewLong(t, p->x * p->x + p->y * p->y);
}

static JSValue pointMoveBy(JSTask t, APTR self, LONG argc, JSValue *argv, APTR data)
{
    struct Point *p = self;
    LONG dx = 0, dy = 0;
    (void)data;
    if (argc < 2)
        return E->Throw(t, (CONST_STRPTR)"moveBy needs dx and dy");
    E->ToLong(t, argv[0], &dx);
    E->ToLong(t, argv[1], &dy);
    p->x += dx;
    p->y += dy;
    return E->Dup(t, argv[0]);
}

static JSValue pointGetX(JSTask t, APTR self, APTR data)
{
    (void)data;
    return E->NewLong(t, ((struct Point *)self)->x);
}

static LONG pointSetX(JSTask t, APTR self, JSValue value, APTR data)
{
    (void)data;
    return E->ToLong(t, value, &((struct Point *)self)->x);
}

static void pointRelease(JSTask t, APTR self, APTR data)
{
    (void)t; (void)data;
    ((struct Point *)self)->released++;
}

static const struct JSHostSlot shapeSlots[] = {
    { (CONST_STRPTR)"visible", JSS_FIELD, JSF_BOOL, NULL, NULL, offsetof(struct Point, visible), 0 },
    { (CONST_STRPTR)"label", JSS_FIELD, JSF_STRPTR, NULL, NULL, offsetof(struct Point, label), 0 },
    { NULL, 0, 0, NULL, NULL, 0, 0 }
};
static const struct JSHostClass shapeClass = { (CONST_STRPTR)"Shape", NULL, shapeSlots, NULL, NULL };

static const struct JSHostSlot pointSlots[] = {
    { (CONST_STRPTR)"x", JSS_ACCESSOR, 0, (APTR)pointGetX, (APTR)pointSetX, 0, 0 },
    { (CONST_STRPTR)"y", JSS_FIELD, JSF_LONG, NULL, NULL, offsetof(struct Point, y), 0 },
    { (CONST_STRPTR)"length2", JSS_METHOD, 0, (APTR)pointLength2, NULL, 0, 0 },
    { (CONST_STRPTR)"moveBy", JSS_METHOD, 0, (APTR)pointMoveBy, NULL, 0, 2 },
    { NULL, 0, 0, NULL, NULL, 0, 0 }
};
static const struct JSHostClass pointClass = { (CONST_STRPTR)"Point", &shapeClass, pointSlots, pointRelease, NULL };

/* ---- Functions on the global object ------------------------------------------- */

static JSValue fnAdd(JSTask t, APTR self, LONG argc, JSValue *argv, APTR data)
{
    LONG a = 0, b = 0;
    (void)self; (void)data;
    if (argc > 0) E->ToLong(t, argv[0], &a);
    if (argc > 1) E->ToLong(t, argv[1], &b);
    return E->NewLong(t, a + b);
}

/* Calls back into script from inside a host function (on the script task) */
static JSValue fnTwice(JSTask t, APTR self, LONG argc, JSValue *argv, APTR data)
{
    JSValue r1 = NULL, r2 = NULL, sum;
    LONG a = 0, b = 0;
    (void)self; (void)data;
    if (argc < 1 || E->TypeOf(t, argv[0]) != JST_FUNCTION)
        return E->Throw(t, (CONST_STRPTR)"twice needs a function");
    E->Call(t, argv[0], NULL, 0, NULL, &r1);
    E->Call(t, argv[0], NULL, 0, NULL, &r2);
    E->ToLong(t, r1, &a);
    E->ToLong(t, r2, &b);
    E->Free(t, r1);
    E->Free(t, r2);
    sum = E->NewLong(t, a + b);
    return sum;
}

static void setGlobalFn(JSTask t, const char *name, JSMethodFunc f)
{
    JSValue g = E->Global(t), fn = E->NewFunction(t, (CONST_STRPTR)name, f, NULL);
    E->SetProp(t, g, (CONST_STRPTR)name, fn);
    E->Free(t, fn);
    E->Free(t, g);
}

/* ---- Jobs --------------------------------------------------------------------------- */

static int jobsRun;

static LONG countJob(JSTask t, APTR data)
{
    (void)t;
    jobsRun += (int)(long)data;
    return 7;
}

/* ---- Several tasks at once ------------------------------------------------------------
 * Three tasks live side by side, driven from three threads. Their scripts
 * take turns (gate): the 68k interpreter keeps its registers in one
 * register file, which AmigaOS swaps at each task switch (LowLevelInterpreter.cpp,
 * tc_Switch and tc_Launch) but Linux, running threads truly at once under
 * qemu, does not. */

static pthread_mutex_t gate = PTHREAD_MUTEX_INITIALIZER;

static void *worker(void *arg)
{
    long n = (long)arg, i;
    char src[96], want[32], text[64];
    struct TagItem tags[] = { { JSA_Name, (ULONG)"worker" }, { TAG_DONE, 0 } };
    JSTask t;
    long good;
    pthread_mutex_lock(&gate);
    t = E->NewTask(tags);
    if (t)
        E->Evaluate(t, (CONST_STRPTR)"0", -1, NULL, NULL);    /* queued after the task's own set-up */
    pthread_mutex_unlock(&gate);
    good = t != NULL;
    for (i = 0; t && i < 20; i++) {
        snprintf(src, sizeof src, "var s = 0; for (var k = 0; k < %ld; k++) s += k; String(s)", 1000 + n + i);
        snprintf(want, sizeof want, "%ld", (1000 + n + i) * (999 + n + i) / 2);
        pthread_mutex_lock(&gate);
        evalText(t, src, text, sizeof text);
        pthread_mutex_unlock(&gate);
        if (strcmp(text, want))
            good = 0;
        usleep(1000);   /* let the other threads in */
    }
    if (t) {
        pthread_mutex_lock(&gate);
        E->EndTask(t);
        pthread_mutex_unlock(&gate);
    }
    return (void *)good;
}

int main(void)
{
    struct TagItem tags[] = { { JSA_StackSize, 512 * 1024 }, { JSA_UserData, 1234 }, { TAG_DONE, 0 } };
    JSTask t;
    JSValue v, g;
    LONG n;
    DOUBLE d;
    char text[256];

    setvbuf(stdout, NULL, _IONBF, 0);
    E = jsl_engine_table();
    check(E->jt_Version == JSL_TABLE_VERSION && E->jt_Size == sizeof *E, "the table's version and size");

    t = E->NewTask(tags);
    check(t != NULL, "a task starts");
    if (!t)
        return 1;
    check(((struct JSTaskHead *)t)->th_Table == E, "a task starts with the table");
    check(E->UserData(t) == (APTR)1234, "JSA_UserData");

    /* Scripts and values */
    check(evalIs(t, "1 + 2", "3"), "evaluate a sum");
    check(evalIs(t, "var a = [3, 1, 2]; a.sort().join()", "1,2,3"), "evaluate an array sort");
    check(evalIs(t, "a.length", "3"), "globals stay between evaluations");
    check(evalIs(t, "JSON.stringify({b: [1, 'two', null]})", "{\"b\":[1,\"two\",null]}"), "JSON");
    check(evalIs(t, "'\xc3\xa9t\xc3\xa9'.length", "3"), "UTF-8 source");
    check(E->Evaluate(t, (CONST_STRPTR)"(1 + ", -1, (CONST_STRPTR)"bad.js", NULL) == JSERR_EXCEPTION, "a syntax error is an exception");
    E->ErrorText(t, (STRPTR)text, sizeof text);
    check(strstr(text, "SyntaxError") != NULL, "its text names the SyntaxError");
    check(E->Evaluate(t, (CONST_STRPTR)"\n\nnull.x", -1, (CONST_STRPTR)"line.js", NULL) == JSERR_EXCEPTION, "a TypeError is an exception");
    E->ErrorText(t, (STRPTR)text, sizeof text);
    check(strncmp(text, "line.js:3: TypeError", 20) == 0, "its text has the file and line");
    check(E->Evaluate(t, (CONST_STRPTR)"6 * 7 trailing junk", 5, NULL, &v) == JSERR_OK, "a source with a length");
    check(E->ToLong(t, v, &n) == JSERR_OK && n == 42, "its value as a LONG");
    E->Free(t, v);

    v = E->NewString(t, (CONST_STRPTR)"hello", -1);
    check(E->TypeOf(t, v) == JST_STRING, "a new string");
    check(E->ToString(t, v, (STRPTR)text, 3) == 5 && strcmp(text, "he") == 0, "ToString cuts to the buffer, gives the length");
    E->Free(t, v);
    d = 2.5;
    v = E->NewDouble(t, &d);
    d = 0;
    check(E->ToDouble(t, v, &d) == JSERR_OK && d == 2.5, "a double there and back");
    E->Free(t, v);
    v = E->NewLong(t, -7);
    check(E->ToLong(t, v, &n) == JSERR_OK && n == -7, "a LONG there and back");
    E->Free(t, v);
    check(E->TypeOf(t, NULL) == JST_UNDEFINED, "NULL is undefined");
    v = E->NewNull(t);
    check(E->TypeOf(t, v) == JST_NULL, "null");
    E->Free(t, v);
    v = E->NewBool(t, 1);
    check(E->ToBool(t, v) == 1 && E->TypeOf(t, v) == JST_BOOLEAN, "a boolean");
    E->Free(t, v);
    E->Evaluate(t, (CONST_STRPTR)"4294967301", -1, NULL, &v);
    check(E->ToLong(t, v, &n) == JSERR_OK && n == 5, "ToLong wraps as ToInt32");
    E->Free(t, v);

    /* Objects, properties and calls */
    v = E->NewObject(t);
    {
        JSValue s = E->NewString(t, (CONST_STRPTR)"kept", -1), back;
        check(E->SetProp(t, v, (CONST_STRPTR)"name", s) == JSERR_OK, "SetProp");
        back = E->GetProp(t, v, (CONST_STRPTR)"name");
        E->ToString(t, back, (STRPTR)text, sizeof text);
        check(strcmp(text, "kept") == 0, "GetProp");
        E->Free(t, back);
        E->Free(t, s);
    }
    g = E->Global(t);
    E->SetProp(t, g, (CONST_STRPTR)"obj", v);
    check(evalIs(t, "obj.name", "kept"), "an object set on the global object");
    E->Free(t, v);
    E->Evaluate(t, (CONST_STRPTR)"[10, 20, 30]", -1, NULL, &v);
    {
        JSValue e = E->GetIndex(t, v, 1), r = NULL, args[2];
        E->ToLong(t, e, &n);
        check(n == 20, "GetIndex");
        E->Free(t, e);
        e = E->NewLong(t, 99);
        E->SetIndex(t, v, 3, e);
        E->Free(t, e);
        E->SetProp(t, g, (CONST_STRPTR)"arr", v);
        check(evalIs(t, "arr.join('-')", "10-20-30-99"), "SetIndex");
        E->Free(t, v);
        E->Evaluate(t, (CONST_STRPTR)"(function (a, b) { return a * b + this.k; })", -1, NULL, &v);
        args[0] = E->NewLong(t, 6);
        args[1] = E->NewLong(t, 7);
        e = E->NewObject(t);
        {
            JSValue k = E->NewLong(t, 100);
            E->SetProp(t, e, (CONST_STRPTR)"k", k);
            E->Free(t, k);
        }
        check(E->Call(t, v, e, 2, args, &r) == JSERR_OK, "Call");
        E->ToLong(t, r, &n);
        check(n == 142, "Call with this and two arguments");
        E->Free(t, r);
        E->Free(t, e);
        E->Free(t, args[0]);
        E->Free(t, args[1]);
        E->Free(t, v);
        E->Evaluate(t, (CONST_STRPTR)"(function () { throw new RangeError('no'); })", -1, NULL, &v);
        check(E->Call(t, v, NULL, 0, NULL, &r) == JSERR_EXCEPTION, "a call that throws");
        E->ErrorText(t, (STRPTR)text, sizeof text);
        check(strstr(text, "RangeError: no") != NULL, "its error text");
        E->Free(t, r);
        E->Free(t, v);
    }
    E->Free(t, g);

    /* Functions written in C */
    setGlobalFn(t, "add", fnAdd);
    setGlobalFn(t, "twice", fnTwice);
    check(evalIs(t, "add(40, 2)", "42"), "a C function");
    check(evalIs(t, "typeof add", "function"), "it is a function to scripts");
    check(evalIs(t, "var c = 0; twice(function () { return ++c; }) + ':' + c", "3:2"), "a C function calling back into script");
    check(evalIs(t, "try { twice(5) } catch (e) { e.message }", "twice needs a function"), "a C function throwing");

    /* Host classes */
    {
        struct Point p = { 3, 4, 1, "corner", 0 }, q = { 1, 1, 0, NULL, 0 };
        JSClass cls = E->RegisterClass(t, &pointClass);
        JSValue w1, w2;
        check(cls != NULL, "RegisterClass");
        check(E->RegisterClass(t, &pointClass) == cls, "a class registered twice is the same class");
        w1 = E->Wrap(t, cls, &p);
        w2 = E->Wrap(t, cls, &p);
        g = E->Global(t);
        E->SetProp(t, g, (CONST_STRPTR)"p", w1);
        E->SetProp(t, g, (CONST_STRPTR)"p2", w2);
        check(evalIs(t, "p === p2", "true"), "one wrapper per object");
        check(E->Unwrap(t, w1, cls) == &p, "Unwrap");
        check(evalIs(t, "p.x + ',' + p.y + ',' + p.length2()", "3,4,25"), "accessor, field and method");
        check(evalIs(t, "p.x = 10; p.x", "10") && p.x == 10, "an accessor's setter reaches the program");
        p.y = -2;
        check(evalIs(t, "p.y", "-2"), "a field reads the program's memory each time");
        check(evalIs(t, "p.visible + ' ' + p.label", "true corner"), "the parent class's fields");
        check(evalIs(t, "p.moveBy(1, 2); p.x + ',' + p.y", "11,0"), "a method with arguments");
        check(evalIs(t, "try { p.moveBy() } catch (e) { e.message }", "moveBy needs dx and dy"), "a method throwing");
        check(evalIs(t, "var k = []; for (var n in p) k.push(n); k.join()", "x,y,visible,label"), "accessors and fields enumerate, methods do not");
        check(evalIs(t, "Object.keys(p).length", "0"), "slots live on the prototype, as the DOM's do");
        check(evalIs(t, "try { p.length2.call({}) } catch (e) { e.constructor.name }", "TypeError"), "a method on the wrong object throws");
        check(evalIs(t, "p.extra = 'expando'; p.extra", "expando"), "a wrapper takes script properties");
        E->Free(t, w1);
        E->Free(t, w2);
        w1 = E->Wrap(t, cls, &q);
        E->SetProp(t, g, (CONST_STRPTR)"q", w1);
        E->Free(t, w1);
        check(evalIs(t, "q.label === null && q.visible === false", "true"), "a NULL string field is null");
        /* q's wrapper is dropped by script and collected: its release runs */
        E->Evaluate(t, (CONST_STRPTR)"q = null; var junk = []; for (var i = 0; i < 200000; i++) junk.push({i: i}); junk = null;", -1, NULL, NULL);
        E->Evaluate(t, (CONST_STRPTR)"for (var i = 0; i < 200000; i++) ({i: i});", -1, NULL, NULL);
        printf("     q released %d time(s) by the collector so far\n", q.released);
        E->Free(t, g);

        /* Jobs */
        check(E->Post(t, countJob, (APTR)1, 1) == 7 && jobsRun == 1, "a job run and waited for");
        check(E->Post(t, countJob, (APTR)10, 0) == JSERR_OK, "a job queued");
        check(evalIs(t, "1", "1") && jobsRun == 11, "a queued job runs before the next call");

        E->EndTask(t);
        check(p.released == 1, "ending the task releases the live wrapper");
        check(q.released == 1, "every wrapper is released once");
    }

    /* Several script tasks at once, from several threads */
    {
        pthread_t th[16];
        void *good;
        long all = 1, i;
        long count = 3;
        for (i = 0; i < count; i++)
            pthread_create(&th[i], NULL, worker, (void *)i);
        for (i = 0; i < count; i++) {
            pthread_join(th[i], &good);
            all &= (long)good;
        }
        check(all, "three tasks on three threads");
    }

    printf("%d failure(s)\n", failures);
    return failures;
}
