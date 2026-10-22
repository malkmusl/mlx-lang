// webcore-dump: a page loaded and laid out headless, its render tree
// printed (PORT.md, M3), as WebKit's test runners print a layout test's
// (DumpRenderTree's externalRepresentation, RenderTreeAsText.cpp).
//
//     webcore-dump [--font FILE]... [--size WIDTHxHEIGHT] [--no-script] [--timeout SECONDS] FILE|URL
//
// A Page of the port's clients (MlxFrameLoaderClient, the port's platform
// strategies and loader; WebCore's empty clients for the rest) with a main
// frame of 800x600 (--size) loads the file (a path) or the URL (file: or
// data:), WTF's generic RunLoop runs until the frame has finished loading
// and no load is pending (subresources, fonts), the view is laid out and
// its render tree written to standard output. The settings are those
// WebKitTestRunner gives a test (its fonts' families, sizes and file
// access, glyphs unhinted); --font adds the fonts of a file as the test
// runner's test fonts (Ahem); fonts are matched by std.fontconfig with the environment's
// configuration (FONTCONFIG_FILE). Exit status: 0, 1 when the page did not
// load (still dumped), 2 for a bad command line.
//
// Built by ./mlx build webkit-mlx-webcore-dump (mlx-out/webkit/mlx/bin);
// tools/check_webkit_mlx_layout.sh runs it on WebKit's layout tests.
#include "config.h"

#define private public
#define protected public
#include "LocalFrame.h"
#include <wtf/WeakHashSet.h>
#undef private
#undef protected
#include "CachedResourceLoader.h"
#include "CommonAtomStrings.h"
#include "Document.h"
#include "EmptyClients.h"
#include "FrameLoadRequest.h"
#include "FontRenderOptions.h"
#include "FrameLoader.h"
#include "LocalFrame.h"
#include "LocalFrameInlines.h"
#include "LocalFrameView.h"
#include "Page.h"
#include "PageConfiguration.h"
#include "RenderTreeAsText.h"
#include "ResourceRequest.h"
#include "Settings.h"
#include "port/MlxWebPort.h"
#include "port/embed/MlxFrameLoaderClient.h"
#include "port/embed/MlxPlatformStrategies.h"
#include <JavaScriptCore/InitializeThreading.h>
#include <pal/SessionID.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wtf/FileSystem.h>
#include <wtf/MainThread.h>
#include <wtf/RunLoop.h>
#include <wtf/text/CString.h>
#include <wtf/text/MakeString.h>

using namespace WebCore;

// TEMPORARY DEBUG: a deep backtrace on SIGTRAP/SIGABRT/SIGSEGV/SIGILL.
#include <signal.h>
#include <ucontext.h>
static void debugBacktrace(int signal, siginfo_t*, void* context)
{
    auto* uc = static_cast<ucontext_t*>(context);
    uintptr_t rip = uc->uc_mcontext.gregs[REG_RIP];
    uintptr_t* rbp = reinterpret_cast<uintptr_t*>(uc->uc_mcontext.gregs[REG_RBP]);
    fprintf(stderr, "DEBUG signal %d at %lx\n", signal, (unsigned long)rip);
    for (int i = 0; i < 400 && rbp; ++i) {
        uintptr_t next = rbp[0];
        uintptr_t ret = rbp[1];
        fprintf(stderr, "DEBUG frame %lx\n", (unsigned long)ret);
        if (next <= reinterpret_cast<uintptr_t>(rbp) || next - reinterpret_cast<uintptr_t>(rbp) > 8 * 1024 * 1024)
            break;
        rbp = reinterpret_cast<uintptr_t*>(next);
    }
    _exit(99);
}

namespace WebCore { extern void (*mlxDebugHook)(const char*); }
static LocalFrame* debugFrame;
static void dumpObservers(const char* where)
{
    fprintf(stderr, "DEBUG observers %s:\n", where);
    static bool watching;
    if (!watching && !strcmp(where, "never")) {
        for (auto& weak : debugFrame->m_destructionObservers.m_set) {
            auto* impl = weak.m_impl.get();
            if (impl && impl->template get<FrameDestructionObserver>() == static_cast<FrameDestructionObserver*>(debugFrame->loader().provisionalDocumentLoader())) {
                watching = true;
                watch(impl);
            }
        }
    }
    auto& table = debugFrame->m_destructionObservers.m_set.m_impl;
    fprintf(stderr, "DEBUG   table %p size %u keys %u deleted %u ops %u\n", table.m_table, table.tableSize(), table.keyCount(), table.m_table ? table.deletedCount() : 0, debugFrame->m_destructionObservers.m_operationCountSinceLastCleanup);
    for (unsigned i = 0; i < table.tableSize(); ++i)
        fprintf(stderr, "DEBUG   bucket %u: %p\n", i, reinterpret_cast<void**>(table.m_table)[i]);
    for (auto& weak : debugFrame->m_destructionObservers.m_set) {
        auto* impl = weak.m_impl.get();
        fprintf(stderr, "DEBUG   impl %p refs %u object %p\n", impl, impl ? impl->refCount() : 0, impl ? impl->template get<FrameDestructionObserver>() : nullptr);
    }
}

#include <fcntl.h>
struct RawPerfAttr {
    uint32_t type, size;
    uint64_t config, samplePeriod, sampleType, readFormat, flags;
    uint32_t wakeupEvents, bpType;
    uint64_t bpAddr, bpLen;
    uint64_t rest[8];
};
static unsigned* watched;
static void watchHit(int, siginfo_t*, void* context)
{
    auto* uc = static_cast<ucontext_t*>(context);
    fprintf(stderr, "WATCH value %u\n", watched ? *watched : 0);
    uintptr_t* rbp = reinterpret_cast<uintptr_t*>(uc->uc_mcontext.gregs[REG_RBP]);
    fprintf(stderr, "WATCH hit at %lx\n", (unsigned long)uc->uc_mcontext.gregs[REG_RIP]);
    for (int i = 0; i < 60 && rbp; ++i) {
        uintptr_t next = rbp[0];
        fprintf(stderr, "WATCH frame %lx\n", (unsigned long)rbp[1]);
        if (next <= reinterpret_cast<uintptr_t>(rbp) || next - reinterpret_cast<uintptr_t>(rbp) > 8 * 1024 * 1024)
            break;
        rbp = reinterpret_cast<uintptr_t*>(next);
    }
}
static void watch(void* address)
{
    struct sigaction action { };
    action.sa_sigaction = watchHit;
    action.sa_flags = SA_SIGINFO | SA_RESTART;
    sigaction(SIGRTMIN + 5, &action, nullptr);
    RawPerfAttr attr { };
    attr.type = 5;
    attr.size = 128;
    attr.bpType = 2;
    attr.bpAddr = reinterpret_cast<uintptr_t>(address);
    watched = static_cast<unsigned*>(address);
    attr.bpLen = 4;
    attr.samplePeriod = 1;
    attr.flags = (1 << 5) | (1 << 6);
    int fd = syscall(298, &attr, 0, -1, -1, 0);
    if (fd < 0) { perror("WATCH perf_event_open"); return; }
    fcntl(fd, F_SETFL, 020000);
    fcntl(fd, 10, SIGRTMIN + 5);
    struct { int type; pid_t pid; } owner { 0, (pid_t)syscall(186) };
    fcntl(fd, 15, &owner);
    fprintf(stderr, "WATCH set on %p\n", address);
}

static int usage()
{
    fprintf(stderr, "usage: webcore-dump [--font FILE]... [--size WIDTHxHEIGHT] [--no-script] [--timeout SECONDS] FILE|URL\n");
    return 2;
}

// The page's state as the loop sees it.
struct Run {
    RefPtr<LocalFrame> frame;
    bool loadFinished { false };
    bool loadSucceeded { false };
    unsigned quietChecks { 0 };
    MonotonicTime deadline;
    bool timedOut { false };
};

// Whether the frame's document has loaded with nothing pending: its load
// finished, the parser done, no request of its resource loader (images,
// style sheets, fonts) outstanding.
static bool isQuiet(Run& run)
{
    if (!run.loadFinished)
        return false;
    RefPtr document = run.frame->document();
    if (!document)
        return true;
    if (document->parsing() || document->cachedResourceLoader().requestCount())
        return false;
    return run.frame->loader().isComplete();
}

// Checked every few milliseconds from the loop: once quiet, the view is
// laid out (which may ask for fonts the style did not name before), and
// the loop stops when it is still quiet a few checks after.
static void check(Run& run)
{
    if (MonotonicTime::now() > run.deadline) {
        run.timedOut = true;
        RunLoop::mainSingleton().stop();
        return;
    }
    if (isQuiet(run)) {
        if (RefPtr document = run.frame->document())
            document->updateLayoutIgnorePendingStylesheets();
        if (isQuiet(run) && ++run.quietChecks >= 3) {
            RunLoop::mainSingleton().stop();
            return;
        }
    } else
        run.quietChecks = 0;
    RunLoop::mainSingleton().dispatchAfter(10_ms, [&run] {
        check(run);
    });
}

int main(int argc, char** argv)
{
    IntSize viewSize(800, 600);
    bool script = true;
    double timeout = 60;
    Vector<const char*> fonts;
    const char* target = nullptr;
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--font") && i + 1 < argc)
            fonts.append(argv[++i]);
        else if (!strcmp(argv[i], "--size") && i + 1 < argc) {
            int width = 0;
            int height = 0;
            if (sscanf(argv[++i], "%dx%d", &width, &height) != 2 || width <= 0 || height <= 0)
                return usage();
            viewSize = IntSize(width, height);
        } else if (!strcmp(argv[i], "--no-script"))
            script = false;
        else if (!strcmp(argv[i], "--timeout") && i + 1 < argc)
            timeout = atof(argv[++i]);
        else if (argv[i][0] == '-' || target)
            return usage();
        else
            target = argv[i];
    }
    if (!target)
        return usage();

    if (getenv("MLX_DEBUG_BACKTRACE")) {
        struct sigaction action { };
        action.sa_sigaction = debugBacktrace;
        action.sa_flags = SA_SIGINFO;
        for (int sig : { SIGTRAP, SIGABRT, SIGSEGV, SIGILL, SIGBUS })
            sigaction(sig, &action, nullptr);
    }
    JSC::initialize();
    WTF::initializeMainThread();
    initializeCommonAtomStrings();
    MlxPlatformStrategies::initialize();
    // Glyphs unhinted, at whole pixels, as the test runner has them
    // (WebKitTestRunner's web process, for the GLib ports' expected trees).
    FontRenderOptions::singleton().disableHintingForTesting();

    for (auto* font : fonts) {
        if (!mlxweb_font_add_file(font)) {
            fprintf(stderr, "webcore-dump: no font in %s\n", font);
            return 2;
        }
    }

    // The URL: a scheme's (file:, data:) as it is, else a file's path.
    String argument = String::fromUTF8(target);
    URL url;
    if (argument.startsWith("file:"_s) || argument.startsWith("data:"_s))
        url = URL { argument };
    else {
        char directory[4096];
        String path = argument;
        if (!argument.startsWith('/') && getcwd(directory, sizeof(directory)))
            path = FileSystem::pathByAppendingComponent(String::fromUTF8(directory), argument);
        url = URL::fileURLWithFileSystemPath(path);
    }
    if (!url.isValid()) {
        fprintf(stderr, "webcore-dump: %s is no URL\n", target);
        return 2;
    }

    static MlxFrameLoaderClient* mainClient;
    auto configuration = pageConfigurationWithEmptyClients(std::nullopt, PAL::SessionID::defaultSessionID());
    configuration.mainFrameCreationParameters = PageConfiguration::LocalMainFrameCreationParameters {
        CompletionHandler<UniqueRef<LocalFrameLoaderClient>(LocalFrame&, FrameLoader&)> { [viewSize](auto&, auto& frameLoader) {
            auto client = makeUniqueRefWithoutRefCountedCheck<MlxFrameLoaderClient>(frameLoader, viewSize);
            mainClient = client.ptr();
            return client;
        } },
        SandboxFlags { },
        ReferrerPolicy::EmptyString
    };

    int status = 0;
    {
        Ref page = Page::create(WTF::move(configuration));
        auto& settings = page->settings();
        settings.setScriptEnabled(script);
        settings.setAcceleratedCompositingEnabled(false);
        // (WebKitTestRunner's: TestController::resetPreferencesToConsistentValues.)
        settings.setStandardFontFamily("Times"_s);
        settings.setFixedFontFamily("Courier"_s);
        settings.setSerifFontFamily("Times"_s);
        settings.setSansSerifFontFamily("Helvetica"_s);
        settings.setCursiveFontFamily("Apple Chancery"_s);
        settings.setFantasyFontFamily("Papyrus"_s);
        settings.setPictographFontFamily("Apple Color Emoji"_s);
        settings.setDefaultFontSize(16);
        settings.setDefaultFixedFontSize(13);
        settings.setMinimumFontSize(0);
        settings.setMinimumLogicalFontSize(9);
        settings.setAllowUniversalAccessFromFileURLs(true);
        settings.setAllowFileAccessFromFileURLs(true);
        settings.setUsesBackForwardCache(false);

        Run run;
        run.frame = page->localMainFrame();
        if (!run.frame || !mainClient) {
            fprintf(stderr, "webcore-dump: the page has no main frame\n");
            return 1;
        }
        debugFrame = run.frame.get();
        mlxDebugHook = dumpObservers;
        dumpObservers("before init");
        run.frame->init();
        dumpObservers("after init");
        mainClient->setLoadFinishedCallback([&run](bool succeeded) {
            run.loadFinished = true;
            run.loadSucceeded = succeeded;
        });

        {
            FrameLoadRequest frameLoadRequest(*run.frame, ResourceRequest(URL { url }));
            auto* impl = frameLoadRequest.resourceRequest().url().string().impl();
            fprintf(stderr, "WATCH url string impl %p refs %u\n", impl, impl->refCount());
            auto* victim = reinterpret_cast<StringImpl*>((reinterpret_cast<uintptr_t>(impl) & ~uintptr_t(0xffffff)) + 0x33a90);
            fprintf(stderr, "WATCH victim %p refs %u length %u text %.40s\n", victim, *reinterpret_cast<unsigned*>(victim), victim->length(), victim->is8Bit() ? reinterpret_cast<const char*>(victim->span8().data()) : "(16-bit)");
            watch(victim);
            run.frame->loader().load(WTF::move(frameLoadRequest));
            fprintf(stderr, "WATCH after load\n");
        }
        run.deadline = MonotonicTime::now() + Seconds(timeout);
        RunLoop::mainSingleton().dispatch([&run] {
            check(run);
        });
        RunLoop::run();

        if (run.timedOut)
            fprintf(stderr, "webcore-dump: %s did not finish loading in %g s\n", target, timeout);
        else if (!run.loadSucceeded)
            fprintf(stderr, "webcore-dump: %s did not load\n", target);
        if (run.timedOut || !run.loadSucceeded)
            status = 1;

        String tree = externalRepresentation(run.frame.get());
        auto utf8 = tree.utf8();
        fwrite(utf8.data(), 1, utf8.length(), stdout);
        fflush(stdout);
        mainClient->setLoadFinishedCallback(nullptr);
        run.frame = nullptr;
    }
    return status;
}
