// webcore-dump: a page loaded and laid out headless, its render tree
// printed (PORT.md, M3), as WebKit's test runners print a layout test's
// (DumpRenderTree's externalRepresentation, RenderTreeAsText.cpp), and
// painted (M4).
//
//     webcore-dump [--font FILE]... [--size WIDTHxHEIGHT] [--no-script] [--subpixel] [--boxes] [--text] [--wait-title TITLE] [--timeout SECONDS] [--dark [--force-dark]] [--scheme NAME] [--profile DIR] [--test]
//                  [--png OUT.png [--reference REF.png [--tolerance CHANNEL] [--allow PIXELS]]] FILE|URL
//
// A Page of the port's clients (MlxFrameLoaderClient, the port's platform
// strategies and loader; WebCore's empty clients for the rest) with a main
// frame of 1280x800 (--size; the layout and paint checks give 800x600)
// loads the file (a path) or the URL (file: or data:), WTF's generic
// RunLoop runs until the frame has finished loading and no load is
// pending (subresources, fonts), the view is laid out and its render tree
// written to standard output. The settings are those
// WebKitTestRunner gives a test (its fonts' families, sizes and file
// access, glyphs unhinted); --font adds the fonts of a file as the test
// runner's test fonts (Ahem); fonts are matched by std.fontconfig with the environment's
// configuration (FONTCONFIG_FILE). Exit status: 0, 1 when the page did not
// load (still dumped), 2 for a bad command line.
//
// --dark gives the page the desktop's dark appearance (WebCore's
// Page::setUseColorAppearance, as the GLib ports' web views do when the
// desktop prefers dark): `prefers-color-scheme: dark` matches, and a page
// whose color-scheme allows dark (`color-scheme: light dark`) is drawn in
// its dark variant, the theme's controls, scrollbars and canvas too.
// --force-dark with it paints a page that has no dark design of its own
// (no color-scheme allowing dark, no prefers-color-scheme: dark rule)
// dark, as Voyager's forced dark mode does (the port's MlxDarkMode.h: the
// colours' lightness inverted, raster images as they are).
//
// --scheme NAME registers the internal scheme NAME (the port's
// MlxSchemeHandler, as Voyager registers voyager:) for the network check:
// NAME://page is a page of it (title "scheme page") whose script fetches
// NAME://page/data.txt (text) and NAME://page/missing (a 404) and writes
// what it got; any other URL of it is a 404 page. With --text, every URL
// the scheme was asked for is listed after the text ("scheme NAME: N
// requests", then each), which shows that a web page could not reach it.
//
// --profile DIR keeps what a browser keeps between runs in DIR (the
// port's MlxProfile: local storage, IndexedDB and the cookies in DIR, the
// HTTP cache in DIR/cache); a second run with the same DIR finds them.
// Without it the profile is a temporary directory, removed at the end, so
// that each run starts from nothing (the checks stay hermetic).
//
// --test runs the page as WebKitTestRunner runs a layout test of its own
// (the testRunner object below: dumpAsText, waitUntilDone, notifyDone),
// and prints what it prints: the console's messages, then the text of the
// page (dumpAsText) or its render tree.
//
// --wait-title TITLE waits, after the load, until the page's title is
// TITLE (what its scripts set when done: a WebSocket's messages, timers).
//
// --text prints the page's URL and title and its body's text
// (HTMLElement::innerText) instead of the render tree: what a page's
// scripts found, for the network check.
//
// --png paints the view (the frame's whole viewport, scrollbars included)
// into a Skia raster surface (CPU, N32 premultiplied, on white) through
// GraphicsContextSkia and writes it as a PNG (std.png, the port's
// mlxweb_image_write_png). --reference compares the painting with a PNG
// (decoded by the port's decoder): a pixel differs when a channel of it
// differs by more than CHANNEL (2 by default); more than PIXELS such
// pixels (0 by default) make the exit status 3. The count and the largest
// difference go to standard error.
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
#include "DeprecatedGlobalSettings.h"
#include "Document.h"
#include "HTMLElement.h"
#include "EmptyClients.h"
#include "FrameLoadRequest.h"
#include "FontRenderOptions.h"
#include "FrameSelection.h"
#include "GraphicsContextSkia.h"
#include "port/platform/graphics/mlx/MlxDarkMode.h"
#include "FrameLoader.h"
#include "NavigationScheduler.h"
#include "LocalFrame.h"
#include "LocalFrameInlines.h"
#include "LocalFrameView.h"
#include "Page.h"
#include "PageConfiguration.h"
#include "InlineIteratorInlineBox.h"
#include "InlineIteratorTextBox.h"
#include "LayoutIntegrationLineLayout.h"
#include "RenderInline.h"
#include "RenderText.h"
#include "RenderTreeAsText.h"
#include "RenderView.h"
#include "ResourceRequest.h"
#include "Settings.h"
#include "port/MlxWebPort.h"
#include "port/embed/MlxFrameLoaderClient.h"
#include "CookieJar.h"
#include "port/embed/MlxNetwork.h"
#include "port/embed/MlxWebSocket.h"
#include "SocketProvider.h"
#include "port/embed/MlxPlatformStrategies.h"
#include "port/embed/MlxSchemeHandler.h"
#include "port/embed/MlxProfile.h"
#include "DOMWrapperWorld.h"
#include "FrameTree.h"
#include "ScriptController.h"
#include <JavaScriptCore/JSCJSValueInlines.h>
#include <JavaScriptCore/InitializeThreading.h>
#include <memory>
#include <skia/core/SkCanvas.h>
#include <skia/core/SkSurface.h>
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

// --boxes: after the tree, each inline's boxes and each text's boxes as
// the inline layout made them, unrounded (where the tree's integers come
// from).
static void dumpInlineBoxes(const RenderElement& root)
{
    for (CheckedPtr renderer = root.firstChild(); renderer; renderer = renderer->nextInPreOrder(&root)) {
        if (auto* inlineRenderer = dynamicDowncast<RenderInline>(*renderer)) {
            if (auto* layout = LayoutIntegration::LineLayout::containing(*inlineRenderer)) {
                auto rect = layout->enclosingBorderBoxRectFor(*inlineRenderer);
                printf("border box %s: x %.6f y %.6f width %.6f height %.6f\n", inlineRenderer->renderName().characters(), rect.x().toFloat(), rect.y().toFloat(), rect.width().toFloat(), rect.height().toFloat());
            }
            for (auto box = InlineIterator::lineLeftmostInlineBoxFor(*inlineRenderer); box; box.traverseInlineBoxLineRightward()) {
                auto rect = box->visualRectIgnoringBlockDirection();
                printf("inline box %s: x %.6f y %.6f width %.6f height %.6f\n", inlineRenderer->renderName().characters(), rect.x(), rect.y(), rect.width(), rect.height());
            }
        } else if (auto* text = dynamicDowncast<RenderText>(*renderer)) {
            for (auto& box : InlineIterator::textBoxesFor(*text)) {
                auto rect = box.visualRectIgnoringBlockDirection();
                printf("text box \"%s\": x %.6f y %.6f width %.6f height %.6f\n", box.originalText().substring(box.start(), box.end() - box.start()).toString().utf8().data(), rect.x(), rect.y(), rect.width(), rect.height());
            }
        }
    }
}

static int usage()
{
    fprintf(stderr, "usage: webcore-dump [--font FILE]... [--size WIDTHxHEIGHT] [--no-script] [--subpixel] [--boxes] [--text] [--wait-title TITLE] [--timeout SECONDS] [--dark [--force-dark]] [--scheme NAME] [--profile DIR] [--test] [--png OUT.png [--reference REF.png [--tolerance CHANNEL] [--allow PIXELS]]] FILE|URL\n");
    return 2;
}

// The view painted into PIXELS (WIDTH x HEIGHT words, 0xAARRGGBB
// premultiplied): white, then the frame view as a window paints it.
static bool paintView(LocalFrame& frame, IntSize size, uint32_t* pixels, bool forceDark)
{
    RefPtr view = frame.view();
    if (!view)
        return false;
    view->updateLayoutAndStyleIfNeededRecursive();
    // (The selection's highlight, as a rendering update gives it.)
    frame.selection().updateAppearance();
    auto info = SkImageInfo::MakeN32Premul(size.width(), size.height());
    auto surface = SkSurfaces::WrapPixels(info, pixels, size.width() * sizeof(uint32_t));
    if (!surface)
        return false;
    RefPtr document = frame.document();
    if (forceDark && document && !mlxPageHasDarkDesign(*document)) {
        surface->getCanvas()->clear(mlxForcedDarkColor(SkColors::kWhite));
        MlxDarkModeCanvas dark(*surface->getCanvas());
        GraphicsContextSkia context(dark, RenderingMode::Unaccelerated, RenderingPurpose::Snapshot);
        view->paint(context, IntRect({ }, size));
        return true;
    }
    surface->getCanvas()->clear(SK_ColorWHITE);
    GraphicsContextSkia context(*surface->getCanvas(), RenderingMode::Unaccelerated, RenderingPurpose::Snapshot);
    view->paint(context, IntRect({ }, size));
    return true;
}

// The pixels that differ from the PNG at PATH by more than TOLERANCE in a
// channel; -1 when it cannot be read or is of another size. LARGEST: the
// largest difference of a channel.
static long differingPixels(const char* path, IntSize size, const uint32_t* pixels, int tolerance, int& largest)
{
    largest = 0;
    FILE* file = fopen(path, "rb");
    if (!file)
        return -1;
    Vector<uint8_t> bytes;
    uint8_t chunk[65536];
    size_t count;
    while ((count = fread(chunk, 1, sizeof(chunk), file)) > 0)
        bytes.append(std::span<const uint8_t> { chunk, count });
    fclose(file);
    auto image = mlxweb_image_decode(MLXWEB_IMAGE_PNG, bytes.span().data(), bytes.size());
    if (!image)
        return -1;
    long differing = -1;
    if (mlxweb_image_width(image) == static_cast<size_t>(size.width()) && mlxweb_image_height(image) == static_cast<size_t>(size.height())) {
        differing = 0;
        auto* reference = mlxweb_image_pixels(image);
        for (int i = 0; i < size.width() * size.height(); ++i) {
            // (The painting is opaque; the reference's pixels over white.)
            uint32_t pixel = pixels[i];
            int ours[3] = { static_cast<int>((pixel >> 16) & 255), static_cast<int>((pixel >> 8) & 255), static_cast<int>(pixel & 255) };
            int alpha = reference[i * 4 + 3];
            int worst = 0;
            for (int c = 0; c < 3; ++c) {
                int theirs = (reference[i * 4 + c] * alpha + 255 * (255 - alpha) + 127) / 255;
                worst = std::max(worst, std::abs(theirs - ours[c]));
            }
            largest = std::max(largest, worst);
            if (worst > tolerance)
                ++differing;
        }
    }
    mlxweb_image_destroy(image);
    return differing;
}

// --test: what WebKitTestRunner gives a layout test of its own
// (LayoutTests' dumpAsText tests: js-test.js's, the storage tests): a
// `testRunner` in every frame's window, made by a script of ours when the
// window object is made (MlxFrameLoaderClient's window-cleared callback,
// where WebKitTestRunner's injected bundle puts its own), its state kept
// here across the main frame's navigations: dumpAsText (the text, not the
// render tree), dumpChildFramesAsText, waitUntilDone and notifyDone (the
// dump waits for notifyDone); any other method of it does nothing. The
// console's messages ("CONSOLE MESSAGE: "), alerts, confirms and prompts
// are printed before the dump, as the test runner prints them.
enum TestFlag : int {
    TestDumpAsText = 1,
    TestWaitUntilDone = 2,
    TestDone = 4,
    TestDumpChildFrames = 8,
};

static bool testMode;
static int testFlags;
static StringBuilder testOutput;

static String testRunnerSource()
{
    return makeString(
        "(function () {"
        "  try { if (window.top !== window && window.top.testRunner && window.top.testRunner.__mlx) { window.testRunner = window.top.testRunner; return; } } catch (e) { }"
        "  var flags = "_s, testFlags, ";"
        "  var runner = { __mlx: true,"
        "    __flags: function () { return flags; },"
        "    dumpAsText: function () { flags |= 1; },"
        "    dumpChildFramesAsText: function () { flags |= 9; },"
        "    waitUntilDone: function () { flags |= 2; },"
        "    notifyDone: function () { flags |= 4; } };"
        "  window.testRunner = new Proxy(runner, { get: function (target, name) {"
        "    if (name in target || typeof name === 'symbol') return target[name];"
        "    return function () { return undefined; }; } });"
        "})();"_s);
}

// The flags of the main frame's testRunner (what its scripts asked so far).
static int pollTestFlags(LocalFrame& frame)
{
    auto value = frame.script().executeScriptIgnoringException("window.testRunner && window.testRunner.__mlx ? window.testRunner.__flags() : -1"_s, JSC::SourceTaintedOrigin::Untainted);
    if (value.isInt32() && value.asInt32() >= 0)
        testFlags |= value.asInt32();
    return testFlags;
}

// A message's file: URLs as their last component, as the test runner
// prints them (the expected text has no paths of the machine it ran on).
static String withoutFileURLs(const String& message)
{
    StringBuilder result;
    StringView rest = message;
    while (true) {
        size_t start = rest.find("file://"_s);
        if (start == notFound) {
            result.append(rest);
            break;
        }
        result.append(rest.left(start));
        size_t end = start + 7;
        while (end < rest.length() && !isASCIIWhitespace(rest[end]) && rest[end] != '\'' && rest[end] != '"' && rest[end] != ')' && rest[end] != ':')
            ++end;
        auto url = rest.substring(start, end - start);
        size_t slash = url.reverseFind('/');
        result.append(slash == notFound ? url : url.substring(slash + 1));
        rest = rest.substring(end);
    }
    return result.toString();
}

static Vector<Ref<Page>>& openedPages()
{
    static NeverDestroyed<Vector<Ref<Page>>> pages;
    return pages;
}

static RefPtr<Page> makePage(IntSize, MlxFrameLoaderClient** client, LocalFrame* opener = nullptr);

// The chrome client of --test's pages: the console and the dialogs
// printed, window.open given a page of its own.
class TestChromeClient final : public EmptyChromeClient {
public:
    explicit TestChromeClient(IntSize size)
        : m_size(size)
    {
    }

private:
    void addMessageToConsole(JSC::MessageSource, JSC::MessageLevel, const String& message, unsigned, unsigned, const String&) final
    {
        testOutput.append("CONSOLE MESSAGE: "_s, withoutFileURLs(message), '\n');
    }
    void runJavaScriptAlert(LocalFrame&, const String& message) final
    {
        testOutput.append("ALERT: "_s, message, '\n');
    }
    bool runJavaScriptConfirm(LocalFrame&, const String& message) final
    {
        testOutput.append("CONFIRM: "_s, message, '\n');
        return true;
    }
    bool runJavaScriptPrompt(LocalFrame&, const String& message, const String& defaultValue, String& result) final
    {
        testOutput.append("PROMPT: "_s, message, ", default text: "_s, defaultValue, '\n');
        result = defaultValue;
        return true;
    }
    RefPtr<Page> createWindow(LocalFrame& opener, const String&, const WindowFeatures&, const NavigationAction&) final
    {
        MlxFrameLoaderClient* client = nullptr;
        RefPtr page = makePage(m_size, &client, &opener);
        if (!page)
            return nullptr;
        if (RefPtr frame = page->localMainFrame())
            frame->init();
        if (RefPtr openerPage = opener.page())
            MlxProfile::pageOpened(*openerPage, *page);
        openedPages().append(*page);
        return page;
    }

    IntSize m_size;
};

// What --no-script, --subpixel and --dark ask of every page.
static bool pageScript = true;
static bool pageSubpixel;
static bool pageDark;

// A page of the port's clients with WebKitTestRunner's settings (CLIENT:
// its main frame's loader client).
static RefPtr<Page> makePage(IntSize viewSize, MlxFrameLoaderClient** client, LocalFrame* opener)
{
    auto configuration = pageConfigurationWithEmptyClients(std::nullopt, PAL::SessionID::defaultSessionID());
    MlxProfile::configure(configuration);
    // (A window.open's page: its opener the frame that opened it.)
    configuration.mainFrameOpener = opener;
    if (testMode)
        configuration.chromeClient = makeUniqueRef<TestChromeClient>(viewSize);
    configuration.mainFrameCreationParameters = PageConfiguration::LocalMainFrameCreationParameters {
        CompletionHandler<UniqueRef<LocalFrameLoaderClient>(LocalFrame&, FrameLoader&)> { [viewSize, client](auto&, auto& frameLoader) {
            auto made = makeUniqueRefWithoutRefCountedCheck<MlxFrameLoaderClient>(frameLoader, viewSize);
            *client = made.ptr();
            return made;
        } },
        SandboxFlags { },
        ReferrerPolicy::EmptyString
    };
    Ref page = Page::create(WTF::move(configuration));
    auto& settings = page->settings();
    settings.setScriptEnabled(pageScript);
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
    // (Web storage is the embedder's to allow: the port's, MlxStorage.)
    settings.setLocalStorageEnabled(true);
    settings.setSessionStorageEnabled(true);
    // (WebKit's defaults where WebCore's differ and a render tree shows
    // it: images loaded, text areas resizable.)
    settings.setLoadsImagesAutomatically(true);
    settings.setTextAreasAreResizable(true);
    // --subpixel: the boxes' sizes as they are, not enclosing integer
    // rectangles (RenderTreeAsText does so with the layer-based SVG
    // engine on, for a page with an <svg> root).
    settings.setLayerBasedSVGEngineEnabled(pageSubpixel);
    // (--test: window.open without a click, as the test runner allows it.)
    if (testMode)
        settings.setJavaScriptCanOpenWindowsAutomatically(true);
    if (pageDark)
        page->setUseColorAppearance(true, false);
    return page;
}

// A frame's text and its subframes (dumpChildFramesAsText), as the test
// runner writes them.
static void appendFrameText(StringBuilder& text, LocalFrame& frame, bool children)
{
    RefPtr document = frame.document();
    RefPtr element = document ? document->documentElement() : nullptr;
    text.append(element ? element->innerText() : String(), '\n');
    if (!children)
        return;
    for (RefPtr child = frame.tree().firstChild(); child; child = child->tree().nextSibling()) {
        RefPtr local = dynamicDowncast<LocalFrame>(child.get());
        if (!local)
            continue;
        text.append("\n--------\nFrame: '"_s, local->tree().specifiedName(), "'\n--------\n"_s);
        appendFrameText(text, *local, true);
    }
}

// The page's state as the loop sees it.
struct Run {
    RefPtr<LocalFrame> frame;
    bool loadFinished { false };
    bool loadSucceeded { false };
    unsigned quietChecks { 0 };
    MonotonicTime deadline;
    bool timedOut { false };
    // --wait-title: the title the page's scripts give it when they are
    // done (null: none to wait for).
    String waitTitle;
};

// Whether the frame's document has loaded with nothing pending: its load
// finished, the parser done, no request of its resource loader (images,
// style sheets, fonts) outstanding, no navigation queued or provisional.
static bool isQuiet(Run& run)
{
    if (testMode) {
        // (A window the test opened has a testRunner of its own, whose
        // notifyDone counts too.)
        for (auto& page : openedPages()) {
            if (RefPtr frame = page->localMainFrame())
                pollTestFlags(*frame);
        }
        int flags = pollTestFlags(*run.frame);
        if (flags & TestDone)
            return true;
        if (flags & TestWaitUntilDone)
            return false;
    }
    if (!run.loadFinished)
        return false;
    RefPtr document = run.frame->document();
    if (!document)
        return true;
    if (document->parsing() || document->cachedResourceLoader().requestCount())
        return false;
    if (!run.waitTitle.isNull() && document->title() != run.waitTitle)
        return false;
    // (Nor a navigation on its way: a form a script submitted, a
    // redirect the page scheduled, a load not committed yet.)
    if (run.frame->navigationScheduler().hasQueuedNavigation() || run.frame->loader().provisionalDocumentLoader())
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
    IntSize viewSize(1280, 800);
    bool script = true;
    bool subpixel = false;
    bool boxes = false;
    bool textOnly = false;
    bool dark = false;
    bool forceDark = false;
    const char* waitTitle = nullptr;
    const char* pngPath = nullptr;
    const char* referencePath = nullptr;
    int tolerance = 2;
    long allowed = 0;
    double timeout = 60;
    Vector<const char*> fonts;
    const char* target = nullptr;
    const char* scheme = nullptr;
    const char* profile = nullptr;
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
        else if (!strcmp(argv[i], "--subpixel"))
            subpixel = true;
        else if (!strcmp(argv[i], "--boxes"))
            boxes = true;
        else if (!strcmp(argv[i], "--wait-title") && i + 1 < argc)
            waitTitle = argv[++i];
        else if (!strcmp(argv[i], "--text"))
            textOnly = true;
        else if (!strcmp(argv[i], "--dark"))
            dark = true;
        else if (!strcmp(argv[i], "--force-dark"))
            forceDark = true;
        else if (!strcmp(argv[i], "--png") && i + 1 < argc)
            pngPath = argv[++i];
        else if (!strcmp(argv[i], "--reference") && i + 1 < argc)
            referencePath = argv[++i];
        else if (!strcmp(argv[i], "--tolerance") && i + 1 < argc)
            tolerance = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--allow") && i + 1 < argc)
            allowed = atol(argv[++i]);
        else if (!strcmp(argv[i], "--scheme") && i + 1 < argc)
            scheme = argv[++i];
        else if (!strcmp(argv[i], "--test"))
            testMode = true;
        else if (!strcmp(argv[i], "--profile") && i + 1 < argc)
            profile = argv[++i];
        else if (!strcmp(argv[i], "--timeout") && i + 1 < argc)
            timeout = atof(argv[++i]);
        else if (argv[i][0] == '-' || target)
            return usage();
        else
            target = argv[i];
    }
    if (!target || (referencePath && !pngPath))
        return usage();

    JSC::initialize();
    WTF::initializeMainThread();
    initializeCommonAtomStrings();
    MlxPlatformStrategies::initialize();
    // Glyphs unhinted, at whole pixels, as the test runner has them
    // (WebKitTestRunner's web process, for the GLib ports' expected trees).
    FontRenderOptions::singleton().disableHintingForTesting();
    // Mock scrollbars (15 pixels, never overlaid), as WebKitTestRunner has
    // them for every port's render trees.
    DeprecatedGlobalSettings::setMockScrollbarsEnabled(true);

    for (auto* font : fonts) {
        if (!mlxweb_font_add_file(font)) {
            fprintf(stderr, "webcore-dump: no font in %s\n", font);
            return 2;
        }
    }

    // The profile: --profile's directory (its cache in DIR/cache), else a
    // temporary one, removed at the end, so that a run keeps nothing.
    String profileDirectory;
    bool temporaryProfile = false;
    if (profile) {
        profileDirectory = String::fromUTF8(profile);
        char directory[4096];
        if (!profileDirectory.startsWith('/') && getcwd(directory, sizeof(directory)))
            profileDirectory = FileSystem::pathByAppendingComponent(String::fromUTF8(directory), profileDirectory);
    } else {
        const char* temporary = getenv("TMPDIR");
        CString pattern = makeString(String::fromUTF8(temporary && *temporary ? temporary : "/tmp"), "/webcore-dump.XXXXXX"_s).utf8();
        Vector<char> name(pattern.span());
        name.append(0);
        if (mkdtemp(name.mutableSpan().data())) {
            profileDirectory = String::fromUTF8(name.span().data());
            temporaryProfile = true;
        }
    }
    MlxProfile::open(profileDirectory, profileDirectory.isEmpty() ? String() : FileSystem::pathByAppendingComponent(profileDirectory, "cache"_s));

    // --scheme: the test scheme, every URL asked of it noted.
    static Vector<String> schemeRequests;
    String schemeName = scheme ? String::fromUTF8(scheme) : String();
    if (scheme) {
        MlxSchemeHandler::registerScheme(schemeName, [schemeName](MlxSchemeTask& task) {
            schemeRequests.append(task.url().string());
            String base = makeString(schemeName, "://page"_s);
            String url = task.url().string();
            auto bytes = [](const String& text) {
                auto utf8 = text.utf8();
                return Vector<uint8_t>(std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(utf8.data()), utf8.length()));
            };
            if (url == base) {
                task.respond(200, "text/html"_s, "utf-8"_s, bytes(makeString(
                    "<!DOCTYPE html><title>scheme page</title><p id=out>loading</p><script>const out = []; fetch('"_s, base,
                    "/data.txt').then(r => r.text()).then(t => { out.push('data: ' + t.trim()); return fetch('"_s, base,
                    "/missing'); }).then(r => { out.push('missing: ' + r.status); }, e => { out.push('failed: ' + e); }).then(() => { document.getElementById('out').textContent = out.join(', '); document.title = 'scheme page done'; });</script>"_s)));
            } else if (url == makeString(base, "/data.txt"_s))
                task.respond(200, "text/plain"_s, "utf-8"_s, bytes("the scheme's own data\n"_s));
            else
                task.respond(404, "text/html"_s, "utf-8"_s, bytes("<!DOCTYPE html><title>not found</title><p>No such page.</p>"_s));
        });
    }

    // The URL: a scheme's (file:, data:) as it is, else a file's path.
    String argument = String::fromUTF8(target);
    URL url;
    if (argument.startsWith("file:"_s) || argument.startsWith("data:"_s) || argument.startsWith("http:"_s) || argument.startsWith("https:"_s) || (scheme && argument.startsWith(makeString(schemeName, ':'))))
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
    // Its user agent: the port's with its own name (as Voyager's has
    // "Voyager/0.1").
    MlxFrameLoaderClient::setApplicationNameForUserAgent("webcore-dump"_s, "0.1"_s);
    pageScript = script;
    pageSubpixel = subpixel;
    pageDark = dark;
    if (testMode) {
        MlxFrameLoaderClient::setWindowObjectClearedCallback([](LocalFrame& frame) {
            frame.script().executeScriptIgnoringException(testRunnerSource(), JSC::SourceTaintedOrigin::Untainted);
        });
    }

    int status = 0;
    {
        RefPtr page = makePage(viewSize, &mainClient);
        Run run;
        run.frame = page->localMainFrame();
        if (waitTitle)
            run.waitTitle = String::fromUTF8(waitTitle);
        if (!run.frame || !mainClient) {
            fprintf(stderr, "webcore-dump: the page has no main frame\n");
            return 1;
        }
        run.frame->init();
        mainClient->setLoadFinishedCallback([&run](bool succeeded) {
            run.loadFinished = true;
            run.loadSucceeded = succeeded;
        });

        run.frame->loader().load(FrameLoadRequest(*run.frame, ResourceRequest(URL { url })));
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

        String tree;
        if (testMode) {
            StringBuilder text;
            text.append(testOutput.toString());
            if (testFlags & TestDumpAsText)
                appendFrameText(text, *run.frame, testFlags & TestDumpChildFrames);
            else
                text.append(externalRepresentation(run.frame.get()));
            tree = text.toString();
        } else if (textOnly) {
            RefPtr document = run.frame->document();
            RefPtr body = document ? document->bodyOrFrameset() : nullptr;
            tree = makeString("url "_s, document ? document->url().string() : String(), "\ntitle "_s, document ? document->title() : String(), "\n"_s, body ? body->innerText() : String(), "\n"_s);
        } else
            tree = externalRepresentation(run.frame.get());
        if (textOnly && scheme) {
            StringBuilder list;
            list.append("scheme "_s, schemeName, ": "_s, schemeRequests.size(), " requests\n"_s);
            for (auto& request : schemeRequests)
                list.append("  "_s, request, '\n');
            tree = makeString(tree, list.toString());
        }
        auto utf8 = tree.utf8();
        fwrite(utf8.data(), 1, utf8.length(), stdout);
        if (boxes && run.frame->contentRenderer())
            dumpInlineBoxes(*run.frame->contentRenderer());
        fflush(stdout);
        if (pngPath) {
            auto pixels = std::make_unique<uint32_t[]>(static_cast<size_t>(viewSize.width()) * viewSize.height());
            if (!paintView(*run.frame, viewSize, pixels.get(), dark && forceDark) || !mlxweb_image_write_png(pngPath, pixels.get(), viewSize.width(), viewSize.height(), viewSize.width(), 0)) {
                fprintf(stderr, "webcore-dump: cannot paint the view into %s\n", pngPath);
                status = 1;
            } else if (referencePath) {
                int largest = 0;
                long differing = differingPixels(referencePath, viewSize, pixels.get(), tolerance, largest);
                if (differing < 0) {
                    fprintf(stderr, "webcore-dump: %s is no PNG of %dx%d\n", referencePath, viewSize.width(), viewSize.height());
                    status = 3;
                } else {
                    fprintf(stderr, "webcore-dump: %ld pixels differ from %s by more than %d (largest difference %d)\n", differing, referencePath, tolerance, largest);
                    if (differing > allowed)
                        status = 3;
                }
            }
        }
        mainClient->setLoadFinishedCallback(nullptr);
        run.frame = nullptr;
    }
    MlxProfile::close();
    if (temporaryProfile)
        FileSystem::deleteNonEmptyDirectory(profileDirectory);
    return status;
}
