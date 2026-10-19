// WebCore of the Mlx port linked whole (PORT.md, M2): JavaScriptCore and
// WTF set up, the port's platform strategies, then a Page with the empty
// clients (EmptyClients.cpp, as WebCore's own TestPage.cpp makes one), its
// main frame given a view and its first document (about:blank). Built by
// ./mlx build webkit-mlx-webcore-test; tools/check_webkit_mlx_port.sh
// checks that it links with nothing undefined but what mlxlibc, mlxlibcpp
// and the Mlx libraries define, runs it and compares what it prints with
// tests/webcore-mlx.expected.
#include "config.h"

#include "CommonAtomStrings.h"
#include "Document.h"
#include "EmptyClients.h"
#include "LocalFrame.h"
#include "LocalFrameInlines.h"
#include "LocalFrameView.h"
#include "Page.h"
#include "PageConfiguration.h"
#include "Settings.h"
#include "port/embed/MlxPlatformStrategies.h"
#include <JavaScriptCore/InitializeThreading.h>
#include <pal/SessionID.h>
#include <stdio.h>
#include <wtf/MainThread.h>
#include <wtf/text/MakeString.h>

using namespace WebCore;

static void say(const String& text)
{
    printf("%s\n", text.utf8().data());
    fflush(stdout);
}

static String yesNo(bool value)
{
    return value ? "yes"_s : "no"_s;
}

int main()
{
    JSC::initialize();
    WTF::initializeMainThread();
    initializeCommonAtomStrings();
    MlxPlatformStrategies::initialize();
    say("initialized"_s);

    {
        Ref page = Page::create(pageConfigurationWithEmptyClients(std::nullopt, PAL::SessionID::defaultSessionID()));
        page->settings().setScriptEnabled(false);
        page->settings().setAcceleratedCompositingEnabled(false);
        say("page: created"_s);

        RefPtr frame = page->localMainFrame();
        say(makeString("main frame: "_s, yesNo(!!frame)));
        if (!frame)
            return 1;
        frame->setView(LocalFrameView::create(*frame, IntSize(800, 600)));
        frame->init();
        RefPtr view = frame->view();
        say(makeString("view: "_s, view ? view->frameRect().width() : 0, 'x', view ? view->frameRect().height() : 0));

        RefPtr document = frame->document();
        say(makeString("document: "_s, yesNo(!!document)));
        if (document)
            say(makeString("url: "_s, document->url().string()));
    }
    say("page: destroyed"_s);
    say("exit 0"_s);
    return 0;
}
