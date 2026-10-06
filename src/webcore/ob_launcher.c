/*
 * OpenBrowser's launcher: the small OpenBrowser program that the icon (or a
 * Shell) starts. The browser itself, PROGDIR:OpenBrowser.engine, is about
 * 100 MB and takes a while to load from disk, so the launcher first opens a
 * title window, then loads the engine with a progress bar, then runs it in
 * this same process: the engine gets this program's icon and tool types (or
 * its Shell arguments) and PROGDIR:. The engine reports its own steps to the
 * title window (ob_splash.h) and closes it once its window is open.
 * MIT, Copyright (c) 2026 Dalsin Limited.
 */
#include <stdio.h>
#include <string.h>
#include <exec/execbase.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <dos/dostags.h>
#include <graphics/gfxbase.h>
#include <graphics/text.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <workbench/startup.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/graphics.h>
#include <proto/intuition.h>

#include "ob_splash.h"

static const char version[] __attribute__((used)) = "$VER: OpenBrowser 0.5 (6.10.2026)";

#define ENGINE "PROGDIR:OpenBrowser.engine"
#define ENGINE_STACK 400000

struct IntuitionBase *IntuitionBase;
struct GfxBase *GfxBase;

static struct Task *mainTask;
static volatile int splashDone;  /* set, under Forbid, as the title window's process ends */

/* ---- The title window, in a process of its own so that it stays alive
 * while the main process is busy loading. */

static struct Window *window;
static struct DrawInfo *drawInfo;
static char statusText[80] = "Loading OpenBrowser";
static LONG percent;

static void drawText(struct RastPort *rp, const char *text, WORD x, WORD y, WORD width)
{
    struct TextExtent extent;
    ULONG fits = TextFit(rp, (CONST_STRPTR)text, strlen(text), &extent, NULL, 1, width, rp->TxHeight + 1);
    Move(rp, x, y + rp->TxBaseline);
    Text(rp, (CONST_STRPTR)text, fits);
}

static void drawWindow(void)
{
    struct RastPort *rp = window->RPort;
    UWORD *pens = drawInfo->dri_Pens;
    WORD left = window->BorderLeft + 10, top = window->BorderTop + 6;
    WORD width = window->Width - window->BorderLeft - window->BorderRight - 20;
    WORD line = rp->TxHeight + 4, barTop = top + 2 * line + 2, barHeight = rp->TxHeight;
    WORD filled = (WORD)((width - 4) * (percent < 0 ? 0 : percent > 100 ? 100 : percent) / 100);

    SetDrMd(rp, JAM1);
    SetAPen(rp, pens[BACKGROUNDPEN]);
    RectFill(rp, window->BorderLeft, window->BorderTop, window->Width - window->BorderRight - 1,
        window->Height - window->BorderBottom - 1);
    SetAPen(rp, pens[TEXTPEN]);
    drawText(rp, version + 6, left, top, width);
    drawText(rp, statusText, left, top + line, width);
    /* A recessed box with the bar inside. */
    SetAPen(rp, pens[SHADOWPEN]);
    RectFill(rp, left, barTop, left + width - 1, barTop);
    RectFill(rp, left, barTop, left, barTop + barHeight - 1);
    SetAPen(rp, pens[SHINEPEN]);
    RectFill(rp, left + 1, barTop + barHeight - 1, left + width - 1, barTop + barHeight - 1);
    RectFill(rp, left + width - 1, barTop + 1, left + width - 1, barTop + barHeight - 1);
    if (filled > 0) {
        SetAPen(rp, pens[FILLPEN]);
        RectFill(rp, left + 2, barTop + 2, left + 1 + filled, barTop + barHeight - 3);
    }
}

static void splashMain(void)
{
    struct MsgPort *port = CreateMsgPort();
    struct Screen *screen = LockPubScreen(NULL);
    int open = 1;

    if (port && screen) {
        WORD fontHeight = screen->Font->ta_YSize, width = 340;
        WORD height = screen->WBorTop + fontHeight + 1 + 6 + 2 * (fontHeight + 4) + 2 + fontHeight + 10 + screen->WBorBottom;
        port->mp_Node.ln_Name = (char *)OB_SPLASH_PORT;
        port->mp_Node.ln_Pri = 0;
        AddPort(port);
        drawInfo = GetScreenDrawInfo(screen);
        window = OpenWindowTags(NULL, WA_PubScreen, (ULONG)screen,
            WA_Left, (screen->Width - width) / 2, WA_Top, (screen->Height - height) / 2,
            WA_Width, width, WA_Height, height, WA_Title, (ULONG)"OpenBrowser",
            WA_DragBar, TRUE, WA_DepthGadget, TRUE, WA_SmartRefresh, TRUE, WA_Activate, TRUE,
            WA_IDCMP, IDCMP_REFRESHWINDOW, TAG_DONE);
    }
    if (screen)
        UnlockPubScreen(NULL, screen);
    if (window)
        drawWindow();
    Signal(mainTask, SIGBREAKF_CTRL_F);

    while (open && window) {
        ULONG got = Wait((1UL << port->mp_SigBit) | (1UL << window->UserPort->mp_SigBit) | SIGBREAKF_CTRL_C);
        struct OBSplashMessage *message;
        struct IntuiMessage *intuiMessage;
        int changed = 0;
        while ((message = (struct OBSplashMessage *)GetMsg(port))) {
            if (message->percent == OB_SPLASH_CLOSE)
                open = 0;
            else {
                if (message->text[0])
                    strcpy(statusText, message->text);
                percent = message->percent;
                changed = 1;
            }
            FreeVec(message);
        }
        while ((intuiMessage = (struct IntuiMessage *)GetMsg(window->UserPort))) {
            if (intuiMessage->Class == IDCMP_REFRESHWINDOW) {
                BeginRefresh(window);
                drawWindow();
                EndRefresh(window, TRUE);
            }
            ReplyMsg((struct Message *)intuiMessage);
        }
        if (changed && open)
            drawWindow();
        if (got & SIGBREAKF_CTRL_C)
            open = 0;
    }

    if (port) {
        struct Message *message;
        if (port->mp_Node.ln_Name) {
            Forbid();
            RemPort(port);
            Permit();
        }
        while ((message = GetMsg(port)))
            FreeVec(message);
        DeleteMsgPort(port);
    }
    if (window)
        CloseWindow(window);
    if (drawInfo) {
        struct Screen *screen = LockPubScreen(NULL);
        if (screen) {
            FreeScreenDrawInfo(screen, drawInfo);
            UnlockPubScreen(NULL, screen);
        }
    }
    /* Under Forbid until this process has gone: its code is the launcher's,
     * which the main process unloads when it ends. */
    Forbid();
    splashDone = 1;
    Signal(mainTask, SIGBREAKF_CTRL_E);
}

/* ---- Loading the engine, with the bar following the bytes read. */

static LONG engineBytes, loadedBytes, lastShown = -1;

static void showLoading(void)
{
    LONG shown = engineBytes ? (LONG)((double)loadedBytes * 70 / engineBytes) : 0;
    char text[80];
    if (shown == lastShown)
        return;
    lastShown = shown;
    snprintf(text, sizeof text, "Loading OpenBrowser: %ld of %ld MB", (long)(loadedBytes >> 20), (long)(engineBytes >> 20));
    ob_splash(text, (int)shown);
}

static LONG readFunction(BPTR file __asm("d1"), APTR buffer __asm("d2"), LONG length __asm("d3"),
    struct DosLibrary *dosBase __asm("a6"))
{
    LONG done = 0;
    (void)dosBase;
    while (done < length) {
        LONG chunk = length - done > 262144 ? 262144 : length - done;
        LONG got = Read(file, (UBYTE *)buffer + done, chunk);
        if (got < 0)
            return done ? done : got;
        done += got;
        loadedBytes += got;
        showLoading();
        if (got < chunk)
            break;
    }
    return done;
}

static APTR allocFunction(ULONG size __asm("d0"), ULONG flags __asm("d1"), struct ExecBase *execBase __asm("a6"))
{
    (void)execBase;
    return AllocMem(size, flags);
}

static void freeFunction(APTR memory __asm("a1"), ULONG size __asm("d0"), struct ExecBase *execBase __asm("a6"))
{
    (void)execBase;
    FreeMem(memory, size);
}

static const LONG loadFunctions[3] = { (LONG)readFunction, (LONG)allocFunction, (LONG)freeFunction };

static BPTR loadEngine(void)
{
    BPTR file = Open((CONST_STRPTR)ENGINE, MODE_OLDFILE), segments = 0;
    struct FileInfoBlock *info = AllocDosObject(DOS_FIB, NULL);
    LONG stack = 0;
    if (file && info && ExamineFH(file, info))
        engineBytes = info->fib_Size;
    if (info)
        FreeDosObject(DOS_FIB, info);
    if (file) {
        showLoading();
        segments = InternalLoadSeg(file, 0, loadFunctions, &stack);
        Close(file);
        if ((LONG)segments < 0)  /* an overlaid program: not ours */
            segments = 0;
    }
    return segments;
}

int main(int argc, char **argv)
{
    struct Process *self = (struct Process *)FindTask(NULL);
    struct Task *splash = NULL;
    struct MsgPort *replyPort = NULL;
    struct WBStartup *engineStartup = NULL;
    BPTR segments;
    LONG result = RETURN_FAIL;

    mainTask = &self->pr_Task;
    IntuitionBase = (struct IntuitionBase *)OpenLibrary((CONST_STRPTR)"intuition.library", 39);
    GfxBase = (struct GfxBase *)OpenLibrary((CONST_STRPTR)"graphics.library", 39);
    if (IntuitionBase && GfxBase && !FindPort((CONST_STRPTR)OB_SPLASH_PORT)) {
        SetSignal(0, SIGBREAKF_CTRL_E | SIGBREAKF_CTRL_F);
        splash = (struct Task *)CreateNewProcTags(NP_Entry, (ULONG)splashMain, NP_Name, (ULONG)"OpenBrowser title",
            NP_StackSize, 8192, NP_Priority, 1, TAG_DONE);
        if (splash)
            Wait(SIGBREAKF_CTRL_F | SIGBREAKF_CTRL_E);
    }

    segments = loadEngine();
    if (!segments) {
        ob_splash("OpenBrowser.engine is missing from this drawer", 0);
        if (argc)
            printf("OpenBrowser: cannot load %s\n", ENGINE);
        Delay(5 * TICKS_PER_SECOND);
    } else {
        ob_splash("Starting OpenBrowser", 70);
        if (argc == 0) {
            /* From Workbench: the engine's startup code waits for a
             * Workbench message; hand it a copy of ours, so it finds the
             * same icon, tool types and drawer. */
            struct WBStartup *ours = (struct WBStartup *)argv;
            replyPort = CreateMsgPort();
            engineStartup = AllocVec(sizeof *engineStartup, MEMF_PUBLIC | MEMF_CLEAR);
            if (replyPort && engineStartup) {
                *engineStartup = *ours;
                engineStartup->sm_Message.mn_Node.ln_Type = NT_MESSAGE;
                engineStartup->sm_Message.mn_ReplyPort = replyPort;
                engineStartup->sm_Message.mn_Length = sizeof *engineStartup;
                engineStartup->sm_Process = &self->pr_MsgPort;
                engineStartup->sm_Segment = segments;
                PutMsg(&self->pr_MsgPort, &engineStartup->sm_Message);
                result = RunCommand(segments, ENGINE_STACK, (CONST_STRPTR)"\n", 1);
                /* The engine's exit code replies the copy. */
                WaitPort(replyPort);
                GetMsg(replyPort);
            }
        } else {
            STRPTR arguments = GetArgStr();
            result = RunCommand(segments, ENGINE_STACK, arguments ? arguments : (STRPTR)"\n",
                arguments ? strlen((char *)arguments) : 1);
        }
        InternalUnLoadSeg(segments, (void (*)(void))freeFunction);
    }

    if (splash) {
        /* The engine closed the title window when its own window opened;
         * otherwise close it now, and wait until its process has gone. */
        ob_splash(NULL, OB_SPLASH_CLOSE);
        Forbid();
        while (!splashDone)
            Wait(SIGBREAKF_CTRL_E);
        Permit();
    }
    if (engineStartup)
        FreeVec(engineStartup);
    if (replyPort)
        DeleteMsgPort(replyPort);
    if (GfxBase)
        CloseLibrary((struct Library *)GfxBase);
    if (IntuitionBase)
        CloseLibrary((struct Library *)IntuitionBase);
    return result < 0 ? RETURN_FAIL : result;
}
