// Voyager's engine (PORT.md, M5): WebCore in Voyager's process, as the
// port's single-process embedding has it. This is the program's main: it
// keeps the tabs, each a Page of the port's clients (MlxFrameLoaderClient,
// the port's platform strategies; a chrome client that notes what needs
// painting and opens new tabs for window.open and `target=_blank`;
// WebCore's empty clients for the rest), and hands the window to
// voyager_run (projects/voyager/voyager.mlx, libvoyager.so), which owns the
// Wayland window (std.ui.host: wl_shm buffers, keys, clipboard) and calls
// back through the table of functions below: open, close and show a tab;
// load a URL in one; run WebCore's pending work (WTF's generic RunLoop,
// one cycle, every tab's: background tabs keep running); paint the shown
// tab into the window's pixels (Skia raster through GraphicsContextSkia,
// the only tab painted), and the pointer's, the wheel's and the keyboard's
// events to it (WebCore's EventHandler; text typed into an editable
// element through the Editor, editing keys as its commands; a link
// clicked with the middle button or Ctrl opens in a new tab behind), the
// selection's text and pasting, each tab's title, URL and loading,
// reloading and stopping, the desktop's colour scheme (dark: the Pages'
// dark appearance, so `prefers-color-scheme: dark` matches and a page that
// allows it is drawn dark, the theme's controls too, and a page without a
// dark design of its own is painted dark: the port's forced dark mode,
// MlxDarkMode.h). Its internal pages are the voyager: scheme (the port's
// MlxSchemeHandler: no web page can reach them): voyager://, voyager://about
// and voyager://version, made by pages.mlx (voyager_page) from the facts
// gathered here; voyager://about/icon.png is the icon the build put in
// (VoyagerBuild.h). about:about and about:version (any about: but blank
// and srcdoc) are voyager://about and voyager://version.
//
//     voyager [--font FILE]... [--size WIDTHxHEIGHT] FILE|URL
//
// Built by ./mlx build webkit-mlx-voyager (mlx-out/webkit/mlx/bin/voyager);
// tools/check_voyager.sh runs it on the compositor.
#include "config.h"

#include "Chrome.h"
#include "DeprecatedGlobalSettings.h"
#include "Document.h"
#include "Element.h"
#include "HTMLElement.h"
#include "FrameSelection.h"
#include "HitTestResult.h"
#include "HitTestRequest.h"
#include "Editor.h"
#include "EditorClient.h"
#include "EmptyClients.h"
#include "KeyboardEvent.h"
#include "SpellChecker.h"
#include "TextCheckerClient.h"
#include "UndoStep.h"
#include "EventHandler.h"
#include "EventNames.h"
#include "FocusController.h"
#include "FontRenderOptions.h"
#include "FrameLoadRequest.h"
#include "FrameLoader.h"
#include "GraphicsContextSkia.h"
#include "HandleUserInputEventResult.h"
#include "LocalFrame.h"
#include "LocalFrameInlines.h"
#include "LocalFrameView.h"
#include "NavigationAction.h"
#include "WindowFeatures.h"
#include "port/platform/graphics/mlx/MlxDarkMode.h"
#include "Page.h"
#include "PageConfiguration.h"
#include "PlatformKeyboardEvent.h"
#include "PlatformMouseEvent.h"
#include "PlatformWheelEvent.h"
#include "ResourceRequest.h"
#include "ScrollingCoordinatorTypes.h"
#include "Settings.h"
#include "CommonAtomStrings.h"
#include "port/MlxWebPort.h"
#include "port/embed/MlxFrameLoaderClient.h"
#include "UserAgent.h"
#include "port/platform/mlx/UserAgentMlx.h"
#include "CookieJar.h"
#include "port/embed/MlxNetwork.h"
#include "port/embed/MlxWebSocket.h"
#include "SocketProvider.h"
#include "port/embed/MlxPlatformStrategies.h"
#include "port/embed/MlxSchemeHandler.h"
#include "port/embed/MlxProfile.h"
#include "port/embed/MlxDownloads.h"
#include "FileChooser.h"
#include <wtf/FileSystem.h>
#include <hb-version.h>
#include <libxml/xmlversion.h>
#include <skia/core/SkMilestone.h>
#include <sqlite3.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <time.h>
#include <unicode/uvernum.h>
#include <zlib.h>
#if __has_include("voyager/VoyagerBuild.h")
#include "voyager/VoyagerBuild.h"
#else
#define VOYAGER_GIT_COMMIT ""
#define VOYAGER_CODEGEN_EPOCH ""
static const unsigned char voyagerIcon[] = { 0 };
#endif
#include <JavaScriptCore/InitializeThreading.h>
#include <pal/SessionID.h>
#include <skia/core/SkCanvas.h>
#include <skia/core/SkSurface.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <utility>
#include <wtf/FileSystem.h>
#include <wtf/HashMap.h>
#include <wtf/MainThread.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/RunLoop.h>
#include <wtf/text/CString.h>

using namespace WebCore;

namespace {

// What Voyager's window asks of the engine: the functions of
// VoyagerEngine (voyager.mlx declares the same table). TAB is a tab's id
// (from open, or from opened for a tab the engine opened itself); the
// events and paint go to the tab shown (select).
struct Engine;

struct VoyagerEngine {
    Engine* engine;
    int (*open)(Engine*, const char* url);
    void (*close)(Engine*, int tab);
    void (*select)(Engine*, int tab);
    int (*opened)(Engine*, int* foreground);
    int (*load)(Engine*, int tab, const char* url);
    void (*resize)(Engine*, int width, int height);
    int (*cycle)(Engine*);
    void (*paint)(Engine*, uint32_t* pixels, int width, int height, int stride);
    void (*pointer)(Engine*, int kind, int x, int y, int button, unsigned modifiers);
    void (*wheel)(Engine*, int x, int y, int deltaX, int deltaY);
    void (*key)(Engine*, int down, unsigned code, const char* text, unsigned modifiers);
    char* (*selectedText)(Engine*);
    void (*insertText)(Engine*, const char* text);
    char* (*title)(Engine*, int tab);
    char* (*url)(Engine*, int tab);
    void (*release)(char*);
    void (*reload)(Engine*, int tab);
    void (*stop)(Engine*, int tab);
    int (*loading)(Engine*, int tab);
    void (*setDark)(Engine*, int dark);
    int (*forcedDark)(Engine*, int tab);
    // A page's file chooser waiting (<input type=file>): what it accepts
    // (its MIME types and extensions, comma-separated; "" for anything),
    // MULTIPLE set when it takes several files; null when none waits.
    // choose answers it: PATHS one a line, or null to cancel.
    char* (*chooser)(Engine*, int* multiple);
    void (*choose)(Engine*, const char* paths);
    // Where downloads are saved (Voyager's XDG_DOWNLOAD_DIR), and the
    // downloads: how many, and each one's name (released), its bytes so
    // far and expected (0: not known), its state (0 running, 1 done, 2
    // failed).
    void (*setDownloads)(Engine*, const char* directory);
    int (*downloadCount)(Engine*);
    char* (*download)(Engine*, int index, uint64_t* received, uint64_t* expected, int* state);
};

// The modifiers as voyager.mlx gives them.
constexpr unsigned modifierShift = 1;
constexpr unsigned modifierControl = 2;
constexpr unsigned modifierAlt = 4;
constexpr unsigned modifierMeta = 8;

// The pointer's events as voyager.mlx gives them.
constexpr int pointerMotion = 0;
constexpr int pointerPress = 1;
constexpr int pointerRelease = 2;

struct Tab;
Tab* openTab(Engine&, bool foreground, LocalFrame* opener = nullptr);

// The chrome client of a tab: what WebCore repaints marks the tab dirty
// (the window paints it again on its next frame if it is shown); a new
// window (window.open, a `target=_blank` link: MlxFrameLoaderClient's
// dispatchCreatePage) is a new tab, shown.
class VoyagerChromeClient final : public EmptyChromeClient {
public:
    VoyagerChromeClient(Engine& engine, bool& dirty)
        : m_engine(engine)
        , m_dirty(dirty)
    {
    }

private:
    void invalidateContentsAndRootView(const IntRect&) final { m_dirty = true; }
    RefPtr<Page> createWindow(LocalFrame&, const String&, const WindowFeatures&, const NavigationAction&) final;
    void runOpenPanel(LocalFrame&, FileChooser&) final;

    Engine& m_engine;
    bool& m_dirty;
};

// A key that types nothing: its DOM key, its Windows key code (WebCore's
// key codes are Windows'), and the editing command it is in an editable
// element (none: the page's).
struct EditingKey {
    unsigned code;
    ASCIILiteral key;
    int windowsKeyCode;
    ASCIILiteral command;
};

constexpr EditingKey editingKeys[] = {
    { 14, "Backspace"_s, 0x08, "DeleteBackward"_s },
    { 15, "Tab"_s, 0x09, ""_s },
    { 28, "Enter"_s, 0x0D, "InsertNewline"_s },
    { 1, "Escape"_s, 0x1B, ""_s },
    { 111, "Delete"_s, 0x2E, "DeleteForward"_s },
    { 105, "ArrowLeft"_s, 0x25, "MoveLeft"_s },
    { 103, "ArrowUp"_s, 0x26, "MoveUp"_s },
    { 106, "ArrowRight"_s, 0x27, "MoveRight"_s },
    { 108, "ArrowDown"_s, 0x28, "MoveDown"_s },
    { 102, "Home"_s, 0x24, "MoveToBeginningOfLine"_s },
    { 107, "End"_s, 0x23, "MoveToEndOfLine"_s },
    { 104, "PageUp"_s, 0x21, "ScrollPageBackward"_s },
    { 109, "PageDown"_s, 0x22, "ScrollPageForward"_s },
};

// The editor client: editing allowed (WebCore's empty one refuses every
// selection and insertion), and the keys' default actions in an editable
// element, as WPE's WebEditorClient has them: a key down runs its editing
// command, a key press types its text. No undo, no spell checking yet.
class VoyagerEditorClient final : public EditorClient {
    WTF_MAKE_TZONE_ALLOCATED_INLINE(VoyagerEditorClient);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(VoyagerEditorClient);
private:
    bool shouldDeleteRange(const std::optional<SimpleRange>&) final { return true; }
    bool smartInsertDeleteEnabled() final { return false; }
    bool isSelectTrailingWhitespaceEnabled() const final { return false; }
    bool isContinuousSpellCheckingEnabled() final { return false; }
    void toggleContinuousSpellChecking() final { }
    bool isGrammarCheckingEnabled() final { return false; }
    void toggleGrammarChecking() final { }
    int spellCheckerDocumentTag() final { return -1; }

    bool shouldBeginEditing(const SimpleRange&) final { return true; }
    bool shouldEndEditing(const SimpleRange&) final { return true; }
    bool shouldInsertNode(Node&, const std::optional<SimpleRange>&, EditorInsertAction) final { return true; }
    bool shouldInsertText(const String&, const std::optional<SimpleRange>&, EditorInsertAction) final { return true; }
    bool shouldChangeSelectedRange(const std::optional<SimpleRange>&, const std::optional<SimpleRange>&, Affinity, bool) final { return true; }

    bool shouldApplyStyle(const StyleProperties&, const std::optional<SimpleRange>&) final { return true; }
    void didApplyStyle() final { }
    bool shouldMoveRangeAfterDelete(const SimpleRange&, const SimpleRange&) final { return true; }

    void didBeginEditing() final { }
    void respondToChangedContents() final { }
    void respondToChangedSelection(LocalFrame*) final { }
    void updateEditorStateAfterLayoutIfEditabilityChanged() final { }
    void discardedComposition(const Document&) final { }
    void canceledComposition() final { }
    void didUpdateComposition() final { }
    void didEndEditing() final { }
    void didEndUserTriggeredSelectionChanges() final { }
    void willWriteSelectionToPasteboard(const std::optional<SimpleRange>&) final { }
    void didWriteSelectionToPasteboard() final { }
    void getClientPasteboardData(const std::optional<SimpleRange>&, Vector<std::pair<String, RefPtr<SharedBuffer>>>&) final { }
    void requestCandidatesForSelection(const VisibleSelection&) final { }
    void handleAcceptedCandidateWithSoftSpaces(TextCheckingResult) final { }

    void registerUndoStep(UndoStep&) final { }
    void registerRedoStep(UndoStep&) final { }
    void clearUndoRedoOperations() final { }

    DOMPasteAccessResponse requestDOMPasteAccess(DOMPasteAccessCategory, FrameIdentifier, const String&) final { return DOMPasteAccessResponse::DeniedForGesture; }

    bool canCopyCut(LocalFrame*, bool defaultValue) const final { return defaultValue; }
    bool canPaste(LocalFrame*, bool defaultValue) const final { return defaultValue; }
    bool canUndo() const final { return false; }
    bool canRedo() const final { return false; }
    void undo() final { }
    void redo() final { }

    void handleKeyboardEvent(KeyboardEvent& event) final
    {
        auto* platformEvent = event.underlyingPlatformEvent();
        RefPtr node = dynamicDowncast<Node>(event.target());
        RefPtr frame = node ? node->document().frame() : nullptr;
        if (!platformEvent || !frame)
            return;
        auto& editor = frame->editor();
        if (event.type() == eventNames().keydownEvent) {
            for (auto& key : editingKeys) {
                if (key.windowsKeyCode == platformEvent->windowsVirtualKeyCode() && !key.command.isEmpty() && String(key.key) == platformEvent->key()) {
                    if (editor.command(String(key.command)).execute(&event))
                        event.setDefaultHandled();
                    return;
                }
            }
            return;
        }
        if (event.type() != eventNames().keypressEvent || platformEvent->controlKey() || platformEvent->altKey() || platformEvent->metaKey())
            return;
        // (A key's control character, a Backspace's or an Enter's, is its
        // key down's command, not text.)
        auto text = platformEvent->text();
        if (text.isEmpty() || text[0] < 0x20 || text[0] == 0x7F)
            return;
        if (editor.insertText(text, &event))
            event.setDefaultHandled();
    }
    void handleInputMethodKeydown(KeyboardEvent&) final { }

    void textFieldDidBeginEditing(Element&) final { }
    void textFieldDidEndEditing(Element&) final { }
    void textDidChangeInTextField(Element&) final { }
    bool doTextFieldCommandFromEvent(Element&, KeyboardEvent*) final { return false; }
    void textWillBeDeletedInTextField(Element&) final { }
    void textDidChangeInTextArea(Element&) final { }
    void overflowScrollPositionChanged() final { }
    void subFrameScrollPositionChanged() final { }

    bool performTwoStepDrop(DocumentFragment&, const SimpleRange&, bool) final { return false; }

#if USE(AUTOMATIC_TEXT_REPLACEMENT)
    void showSubstitutionsPanel(bool) final { }
    bool substitutionsPanelIsShowing() final { return false; }
    void toggleSmartInsertDelete() final { }
    bool isAutomaticQuoteSubstitutionEnabled() final { return false; }
    void toggleAutomaticQuoteSubstitution() final { }
    bool isAutomaticLinkDetectionEnabled() final { return false; }
    void toggleAutomaticLinkDetection() final { }
    bool isAutomaticDashSubstitutionEnabled() final { return false; }
    void toggleAutomaticDashSubstitution() final { }
    bool isAutomaticTextReplacementEnabled() final { return false; }
    void toggleAutomaticTextReplacement() final { }
    bool isAutomaticSpellingCorrectionEnabled() final { return false; }
    void toggleAutomaticSpellingCorrection() final { }
    bool isSmartListsEnabled() final { return false; }
    void toggleSmartLists() { }
#endif

    TextCheckerClient* textChecker() final { return &m_textCheckerClient; }

    void updateSpellingUIWithGrammarString(const String&, const GrammarDetail&) final { }
    void updateSpellingUIWithMisspelledWord(const String&) final { }
    void showSpellingUI(bool) final { }
    bool spellingUIIsShowing() final { return false; }

    void setInputMethodState(Element*) final { }

    class NoTextChecker final : public TextCheckerClient {
        bool shouldEraseMarkersAfterChangeSelection(TextCheckingType) const final { return true; }
        void ignoreWordInSpellDocument(const String&) final { }
        void learnWord(const String&) final { }
        void checkSpellingOfString(StringView, int*, int*) final { }
        void checkGrammarOfString(StringView, Vector<GrammarDetail>&, int*, int*) final { }
#if USE(UNIFIED_TEXT_CHECKING)
        Vector<TextCheckingResult> checkTextOfParagraph(StringView, OptionSet<TextCheckingType>, const VisibleSelection&) final { return { }; }
#endif
        void getGuessesForWord(const String&, const String&, const VisibleSelection&, Vector<String>&) final { }
        void requestCheckingOfString(TextCheckingRequest& request, const VisibleSelection&) final { request.didCancel(); }
        void requestExtendedCheckingOfString(TextCheckingRequest& request, const VisibleSelection&) final { request.didCancel(); }
    };

    NoTextChecker m_textCheckerClient;
};

// A tab: its page, the clicks counted on it, and whether it is to be
// painted again.
struct Tab {
    int id { 0 };
    RefPtr<Page> page;
    RefPtr<LocalFrame> frame;
    MlxFrameLoaderClient* client { nullptr };
    bool dirty { true };
    int clickCount { 0 };
    MonotonicTime lastPress;
    IntPoint lastPressPosition;
    MouseButton pressedButton { MouseButton::None };
    MonotonicTime lastUpdate;
    // A link pressed with the middle button or Ctrl: opened in a tab
    // behind on the release (the page sees neither).
    URL heldLink;
    MouseButton heldLinkButton { MouseButton::None };
};

struct Engine {
    HashMap<int, std::unique_ptr<Tab>> tabs;
    // The tab shown (null: none yet, or it was closed).
    Tab* shown { nullptr };
    int nextId { 1 };
    // The tabs the engine opened itself, for the window: id, and whether
    // shown at once.
    Vector<std::pair<int, bool>> opened;
    IntSize size;
    bool dark { false };
    // VOYAGER_TRACE in the environment: each event on standard error.
    bool trace { false };
    // A page's file chooser waiting for the window's (chooser, choose).
    RefPtr<FileChooser> chooser;
};

Tab* tabOf(Engine* engine, int id)
{
    if (id <= 0)
        return nullptr;
    auto found = engine->tabs.find(id);
    return found == engine->tabs.end() ? nullptr : found->value.get();
}

OptionSet<PlatformEvent::Modifier> modifiersOf(unsigned modifiers)
{
    OptionSet<PlatformEvent::Modifier> result;
    if (modifiers & modifierShift)
        result.add(PlatformEvent::Modifier::ShiftKey);
    if (modifiers & modifierControl)
        result.add(PlatformEvent::Modifier::ControlKey);
    if (modifiers & modifierAlt)
        result.add(PlatformEvent::Modifier::AltKey);
    if (modifiers & modifierMeta)
        result.add(PlatformEvent::Modifier::MetaKey);
    return result;
}

char* copied(const String& string)
{
    auto utf8 = string.utf8();
    char* result = static_cast<char*>(malloc(utf8.length() + 1));
    if (!result)
        return nullptr;
    memcpy(result, utf8.data(), utf8.length());
    result[utf8.length()] = 0;
    return result;
}

URL urlOf(const char* target)
{
    String argument = String::fromUTF8(target);
    // about: pages but WebCore's own (blank, srcdoc) are Voyager's: about:about
    // is voyager://about, about:version voyager://version (as Chrome's
    // about:version is chrome://version).
    if (argument.startsWithIgnoringASCIICase("about:"_s)) {
        String name = argument.substring(6);
        if (!equalLettersIgnoringASCIICase(name, "blank"_s) && !equalLettersIgnoringASCIICase(name, "srcdoc"_s))
            return URL { makeString("voyager://"_s, name) };
        return URL { argument };
    }
    if (argument.startsWith("file:"_s) || argument.startsWith("data:"_s) || argument.startsWith("http:"_s) || argument.startsWith("https:"_s))
        return URL { argument };
    URL schemeURL { argument };
    if (schemeURL.isValid() && MlxSchemeHandler::handles(schemeURL))
        return schemeURL;
    String path = argument;
    char directory[4096];
    if (!argument.startsWith('/') && getcwd(directory, sizeof(directory)))
        path = FileSystem::pathByAppendingComponent(String::fromUTF8(directory), argument);
    return URL::fileURLWithFileSystemPath(path);
}

// A new tab (its page made, its main frame showing nothing yet), noted for
// the window as opened when the engine opened it itself (FOREGROUND: to be
// shown at once).
Tab* makeTab(Engine& engine, LocalFrame* opener)
{
    auto owned = std::make_unique<Tab>();
    Tab* tab = owned.get();
    tab->id = engine.nextId++;
    auto configuration = pageConfigurationWithEmptyClients(std::nullopt, PAL::SessionID::defaultSessionID());
    configuration.chromeClient = makeUniqueRef<VoyagerChromeClient>(engine, tab->dirty);
    configuration.editorClient = makeUniqueRef<VoyagerEditorClient>();
    // The profile's: the cookie jar, web sockets, web storage, IndexedDB.
    MlxProfile::configure(configuration);
    // (A window.open's tab: its opener the frame that opened it.)
    configuration.mainFrameOpener = opener;
    IntSize size = engine.size;
    configuration.mainFrameCreationParameters = PageConfiguration::LocalMainFrameCreationParameters {
        CompletionHandler<UniqueRef<LocalFrameLoaderClient>(LocalFrame&, FrameLoader&)> { [tab, size](auto&, auto& frameLoader) {
            auto client = makeUniqueRefWithoutRefCountedCheck<MlxFrameLoaderClient>(frameLoader, size);
            tab->client = client.ptr();
            return client;
        } },
        SandboxFlags { },
        ReferrerPolicy::EmptyString
    };
    tab->page = Page::create(WTF::move(configuration));
    auto& settings = tab->page->settings();
    settings.setScriptEnabled(true);
    settings.setAcceleratedCompositingEnabled(false);
    settings.setLoadsImagesAutomatically(true);
    settings.setAllowFileAccessFromFileURLs(true);
    settings.setUsesBackForwardCache(false);
    settings.setLocalStorageEnabled(true);
    settings.setSessionStorageEnabled(true);
    settings.setDownloadAttributeEnabled(true);
    tab->frame = tab->page->localMainFrame();
    if (!tab->frame || !tab->client)
        return nullptr;
    tab->frame->init();
    tab->page->setUseColorAppearance(engine.dark, false);
    // With VOYAGER_TRACE, an internal page's text once it has loaded
    // ("voyager: tab N text LINE", each line): what tools/check_voyager.sh
    // reads of voyager://version.
    if (engine.trace) {
        tab->client->setLoadFinishedCallback([tab](bool) {
            RefPtr document = tab->frame->document();
            RefPtr body = document ? document->bodyOrFrameset() : nullptr;
            if (!body || !document->url().protocolIs("voyager"_s))
                return;
            for (auto line : StringView(body->innerText()).split('\n')) {
                if (!line.isEmpty())
                    fprintf(stderr, "voyager: tab %d text %s\n", tab->id, line.utf8().data());
            }
        });
    }
    engine.tabs.add(tab->id, WTF::move(owned));
    if (engine.trace)
        fprintf(stderr, "voyager: engine made tab %d\n", tab->id);
    return tab;
}

Tab* openTab(Engine& engine, bool foreground, LocalFrame* opener)
{
    Tab* tab = makeTab(engine, opener);
    if (tab)
        engine.opened.append({ tab->id, foreground });
    return tab;
}

RefPtr<Page> VoyagerChromeClient::createWindow(LocalFrame& opener, const String&, const WindowFeatures&, const NavigationAction& action)
{
    Tab* tab = openTab(m_engine, true, &opener);
    if (RefPtr openerPage = opener.page(); tab && openerPage)
        MlxProfile::pageOpened(*openerPage, *tab->page);
    if (m_engine.trace)
        fprintf(stderr, "voyager: new window for %s: tab %d\n", action.url().string().utf8().data(), tab ? tab->id : 0);
    return tab ? tab->page : nullptr;
}

// A page asks for files: the window shows its chooser (one at a time:
// a page asking again cancels the one before).
void VoyagerChromeClient::runOpenPanel(LocalFrame&, FileChooser& chooser)
{
    if (RefPtr waiting = std::exchange(m_engine.chooser, &chooser))
        waiting->cancelFileChoosing();
    if (m_engine.trace)
        fprintf(stderr, "voyager: file chooser (%s)\n", chooser.settings().allowsMultipleFiles ? "several files" : "one file");
}

int loadInto(Tab* tab, const char* target)
{
    URL url = urlOf(target);
    if (!tab || !url.isValid())
        return 0;
    tab->frame->loader().load(FrameLoadRequest(*tab->frame, ResourceRequest(URL { url })));
    tab->dirty = true;
    return 1;
}

// A tab of its own (TARGET loaded in it, none when empty): its id, 0
// when it could not be made.
int engineOpen(Engine* engine, const char* target)
{
    Tab* tab = makeTab(*engine, nullptr);
    if (!tab)
        return 0;
    if (target && *target)
        loadInto(tab, target);
    return tab->id;
}

void engineClose(Engine* engine, int id)
{
    Tab* tab = tabOf(engine, id);
    if (!tab)
        return;
    if (engine->trace)
        fprintf(stderr, "voyager: engine closes tab %d\n", id);
    if (engine->shown == tab)
        engine->shown = nullptr;
    tab->frame->loader().stopAllLoaders();
    tab->frame->loader().detachFromParent();
    MlxProfile::pageClosed(*tab->page);
    tab->frame = nullptr;
    tab->page = nullptr;
    engine->tabs.remove(id);
}

// The tab shown: the one painted, given the events, active and focused.
void engineSelect(Engine* engine, int id)
{
    Tab* tab = tabOf(engine, id);
    if (!tab)
        return;
    for (auto& other : engine->tabs.values()) {
        other->page->focusController().setActive(other.get() == tab);
        other->page->focusController().setFocused(other.get() == tab);
    }
    engine->shown = tab;
    tab->dirty = true;
}

// A tab the engine opened itself since the last call (window.open, a
// link's target, a link clicked with the middle button or Ctrl): its id,
// FOREGROUND whether to show it; 0 when there is none.
int engineOpened(Engine* engine, int* foreground)
{
    if (engine->opened.isEmpty())
        return 0;
    auto entry = engine->opened.first();
    engine->opened.removeAt(0);
    if (foreground)
        *foreground = entry.second ? 1 : 0;
    return entry.first;
}

int engineLoad(Engine* engine, int id, const char* target)
{
    return loadInto(tabOf(engine, id), target);
}

void engineResize(Engine* engine, int width, int height)
{
    if (width <= 0 || height <= 0 || IntSize(width, height) == engine->size)
        return;
    engine->size = IntSize(width, height);
    for (auto& tab : engine->tabs.values()) {
        tab->client->setViewSize(engine->size);
        if (RefPtr view = tab->frame->view())
            view->resize(engine->size);
        tab->dirty = true;
    }
}

// One turn of WebCore's loop (timers, loads, tasks: every tab's); 1 when
// the tab shown is to be painted again.
int engineCycle(Engine* engine)
{
    RunLoop::cycle();
    // Each page's rendering update (WebKit's display link: scroll and
    // resize events, animation frames, intersection observers), at most
    // every 30 ms; a tab behind, every 200 ms.
    auto now = MonotonicTime::now();
    for (auto& tab : engine->tabs.values()) {
        auto interval = tab.get() == engine->shown ? 30_ms : 200_ms;
        if (now - tab->lastUpdate >= interval) {
            tab->lastUpdate = now;
            tab->page->isolatedUpdateRendering();
        }
    }
    Tab* shown = engine->shown;
    if (!shown)
        return 0;
    if (RefPtr view = shown->frame->view()) {
        if (view->needsLayout())
            shown->dirty = true;
    }
    return shown->dirty ? 1 : 0;
}

// Whether the tab is painted in forced dark: the desktop dark, its page
// without a dark design of its own (always: Voyager has no setting for
// it, it follows the desktop).
bool isForcedDark(Engine* engine, Tab* tab)
{
    if (!engine->dark || !tab)
        return false;
    RefPtr document = tab->frame->document();
    return document && !mlxPageHasDarkDesign(*document);
}

void enginePaint(Engine* engine, uint32_t* pixels, int width, int height, int stride)
{
    Tab* tab = engine->shown;
    if (!tab)
        return;
    tab->dirty = false;
    RefPtr view = tab->frame->view();
    if (!view)
        return;
    view->updateLayoutAndStyleIfNeededRecursive();
    auto info = SkImageInfo::MakeN32Premul(width, height);
    auto surface = SkSurfaces::WrapPixels(info, pixels, static_cast<size_t>(stride) * sizeof(uint32_t));
    if (!surface)
        return;
    if (isForcedDark(engine, tab)) {
        surface->getCanvas()->clear(mlxForcedDarkColor(SkColors::kWhite));
        MlxDarkModeCanvas dark(*surface->getCanvas());
        GraphicsContextSkia context(dark, RenderingMode::Unaccelerated, RenderingPurpose::DOM);
        view->paint(context, IntRect(0, 0, width, height));
        return;
    }
    surface->getCanvas()->clear(SK_ColorWHITE);
    GraphicsContextSkia context(*surface->getCanvas(), RenderingMode::Unaccelerated, RenderingPurpose::DOM);
    view->paint(context, IntRect(0, 0, width, height));
}

MouseButton buttonOf(int button)
{
    switch (button) {
    case 272: return MouseButton::Left;
    case 273: return MouseButton::Right;
    case 274: return MouseButton::Middle;
    default: return MouseButton::None;
    }
}

void enginePointer(Engine* engine, int kind, int x, int y, int button, unsigned modifiers)
{
    Tab* tab = engine->shown;
    if (!tab)
        return;
    IntPoint position(x, y);
    if (engine->trace && kind != pointerMotion)
        fprintf(stderr, "voyager: pointer %s %d at %d,%d\n", kind == pointerPress ? "press" : "release", button, x, y);
    auto& handler = tab->frame->eventHandler();
    auto now = MonotonicTime::now();
    // A link pressed with the middle button, or with Ctrl: a tab behind
    // for it once the button is let go (the page sees neither).
    if (kind == pointerPress && (buttonOf(button) == MouseButton::Middle || (buttonOf(button) == MouseButton::Left && (modifiers & modifierControl)))) {
        auto result = handler.hitTestResultAtPoint(LayoutPoint(position), { HitTestRequest::Type::ReadOnly, HitTestRequest::Type::Active, HitTestRequest::Type::DisallowUserAgentShadowContent });
        URL link = result.absoluteLinkURL();
        if (!link.isEmpty()) {
            tab->heldLink = link;
            tab->heldLinkButton = buttonOf(button);
            return;
        }
    }
    if (kind == pointerRelease && tab->heldLinkButton != MouseButton::None && buttonOf(button) == tab->heldLinkButton) {
        URL link = std::exchange(tab->heldLink, URL { });
        tab->heldLinkButton = MouseButton::None;
        if (Tab* behind = openTab(*engine, false)) {
            behind->frame->loader().load(FrameLoadRequest(*behind->frame, ResourceRequest(URL { link })));
            if (engine->trace)
                fprintf(stderr, "voyager: link %s in tab %d behind\n", link.string().utf8().data(), behind->id);
        }
        return;
    }
    if (kind == pointerPress) {
        auto pressed = buttonOf(button);
        // A press near the last one, soon after: a double (triple) click.
        if (pressed == tab->pressedButton && now - tab->lastPress < 400_ms && (position - tab->lastPressPosition).diagonalLengthSquared() <= 25)
            tab->clickCount++;
        else
            tab->clickCount = 1;
        tab->lastPress = now;
        tab->lastPressPosition = position;
        tab->pressedButton = pressed;
        handler.handleMousePressEvent(PlatformMouseEvent(position, position, pressed, PlatformEvent::Type::MousePressed, tab->clickCount, modifiersOf(modifiers), now, 0, SyntheticClickType::NoTap, MouseEventInputSource::UserDriven));
    } else if (kind == pointerRelease)
        handler.handleMouseReleaseEvent(PlatformMouseEvent(position, position, buttonOf(button), PlatformEvent::Type::MouseReleased, tab->clickCount, modifiersOf(modifiers), now, 0, SyntheticClickType::NoTap, MouseEventInputSource::UserDriven));
    if (engine->trace && kind == pointerRelease) {
        auto& selection = tab->frame->selection().selection();
        auto result = tab->frame->eventHandler().hitTestResultAtPoint(LayoutPoint(position), { HitTestRequest::Type::ReadOnly, HitTestRequest::Type::Active });
        RefPtr node = result.innerNode();
        fprintf(stderr, "voyager: selection none %d caret %d editable %d; hit %s\n", selection.isNone(), selection.isCaret(), !!selection.rootEditableElement(), node ? node->nodeName().utf8().data() : "nothing");
    }
    if (kind == pointerMotion)
        handler.handleMouseMoveEvent(PlatformMouseEvent(position, position, button ? buttonOf(button) : MouseButton::None, PlatformEvent::Type::MouseMoved, 0, modifiersOf(modifiers), now, 0, SyntheticClickType::NoTap, MouseEventInputSource::UserDriven));
    tab->dirty = true;
}

void engineWheel(Engine* engine, int x, int y, int deltaX, int deltaY)
{
    Tab* tab = engine->shown;
    if (!tab)
        return;
    if (engine->trace)
        fprintf(stderr, "voyager: wheel %d,%d at %d,%d\n", deltaX, deltaY, x, y);
    IntPoint position(x, y);
    // (WebCore's wheel deltas point the other way: positive scrolls up.)
    PlatformWheelEvent event(position, position, -deltaX, -deltaY, -deltaX / 40.0f, -deltaY / 40.0f, PlatformWheelEventGranularity::ScrollByPixelWheelEvent, false, false, false, false);
    tab->frame->eventHandler().handleWheelEvent(event, { WheelEventProcessingSteps::SynchronousScrolling, WheelEventProcessingSteps::BlockingDOMEventDispatch });
    tab->dirty = true;
}

// A key pressed (DOWN 1) or released (0): CODE is its Linux key code, TEXT
// what it types (UTF-8, empty for none). The page sees it first (keydown,
// keypress, keyup); what it leaves undone, the engine does: the text
// typed into the focused editable element, an editing key's command.
void engineKey(Engine* engine, int down, unsigned code, const char* text, unsigned modifiers)
{
    Tab* tab = engine->shown;
    if (!tab)
        return;
    String typed = text ? String::fromUTF8(text) : String();
    if (engine->trace)
        fprintf(stderr, "voyager: key %u %s \"%s\" modifiers %u\n", code, down ? "down" : "up", text ? text : "", modifiers);
    const EditingKey* editing = nullptr;
    for (auto& candidate : editingKeys) {
        if (candidate.code == code)
            editing = &candidate;
    }
    if (editing)
        typed = code == 28 ? "\r"_s : code == 15 ? "\t"_s : code == 14 ? "\b"_s : String();
    String key = editing ? String(editing->key) : typed;
    int windowsKeyCode = editing ? editing->windowsKeyCode : 0;
    if (!editing && typed.length() == 1 && isASCIIAlphanumeric(typed[0]))
        windowsKeyCode = toASCIIUpper(typed[0]);
    auto type = down ? PlatformEvent::Type::KeyDown : PlatformEvent::Type::KeyUp;
    PlatformKeyboardEvent event(type, down ? typed : String(), down ? typed : String(), key, key, key, windowsKeyCode, false, false, false, modifiersOf(modifiers), MonotonicTime::now());
    bool handled = tab->frame->eventHandler().keyEvent(event);
    tab->dirty = true;
    if (engine->trace && down) {
        RefPtr focusedFrame = tab->page->focusController().focusedOrMainFrame();
        RefPtr document = focusedFrame ? focusedFrame->document() : nullptr;
        RefPtr element = document ? document->focusedElement() : nullptr;
        fprintf(stderr, "voyager: key handled %d, focus %s, editable %d\n", handled, element ? element->localName().string().utf8().data() : "none", focusedFrame ? focusedFrame->editor().canEdit() : -1);
    }
    if (!down || handled)
        return;
    RefPtr focused = tab->page->focusController().focusedOrMainFrame();
    if (!focused)
        return;
    auto& editor = focused->editor();
    if (editing) {
        if (!editing->command.isEmpty())
            editor.command(String(editing->command)).execute();
        return;
    }
    if (!typed.isEmpty() && !(modifiers & (modifierControl | modifierAlt | modifierMeta)) && editor.canEdit())
        editor.insertText(typed, nullptr);
}

char* engineSelectedText(Engine* engine)
{
    Tab* tab = engine->shown;
    RefPtr focused = tab ? tab->page->focusController().focusedOrMainFrame() : nullptr;
    String text = focused ? focused->editor().selectedText() : String();
    return text.isEmpty() ? nullptr : copied(text);
}

void engineInsertText(Engine* engine, const char* text)
{
    Tab* tab = engine->shown;
    RefPtr focused = tab ? tab->page->focusController().focusedOrMainFrame() : nullptr;
    if (!focused || !text)
        return;
    auto& editor = focused->editor();
    if (editor.canEdit())
        editor.insertText(String::fromUTF8(text), nullptr);
    tab->dirty = true;
}

char* engineTitle(Engine* engine, int id)
{
    Tab* tab = tabOf(engine, id);
    RefPtr document = tab ? tab->frame->document() : nullptr;
    return copied(document ? document->title() : String());
}

char* engineUrl(Engine* engine, int id)
{
    Tab* tab = tabOf(engine, id);
    RefPtr document = tab ? tab->frame->document() : nullptr;
    return copied(document ? document->url().string() : String());
}

void engineRelease(char* string)
{
    free(string);
}

void engineReload(Engine* engine, int id)
{
    if (Tab* tab = tabOf(engine, id)) {
        tab->frame->loader().reload();
        tab->dirty = true;
    }
}

void engineStop(Engine* engine, int id)
{
    if (Tab* tab = tabOf(engine, id)) {
        tab->frame->loader().stopAllLoaders();
        tab->dirty = true;
    }
}

// 1 while the tab's page (or what it loads) is loading.
int engineLoading(Engine* engine, int id)
{
    Tab* tab = tabOf(engine, id);
    return tab && tab->frame->loader().isLoading() ? 1 : 0;
}

// The desktop's scheme: every page's appearance follows it at once (its
// media queries evaluated again, its styles and the view's background
// resolved again), no reload.
void engineSetDark(Engine* engine, int dark)
{
    if (engine->trace)
        fprintf(stderr, "voyager: appearance %s\n", dark ? "dark" : "light");
    engine->dark = dark;
    for (auto& tab : engine->tabs.values()) {
        tab->page->setUseColorAppearance(dark, false);
        tab->dirty = true;
    }
}

// 1 when the tab is painted in forced dark now.
int engineForcedDark(Engine* engine, int id)
{
    return isForcedDark(engine, tabOf(engine, id)) ? 1 : 0;
}

char* engineChooser(Engine* engine, int* multiple)
{
    if (!engine->chooser)
        return nullptr;
    auto& settings = engine->chooser->settings();
    *multiple = settings.allowsMultipleFiles ? 1 : 0;
    StringBuilder accept;
    for (auto& type : settings.acceptMIMETypes)
        accept.append(accept.isEmpty() ? ""_s : ","_s, type);
    for (auto& extension : settings.acceptFileExtensions)
        accept.append(accept.isEmpty() ? ""_s : ","_s, extension);
    return copied(accept.toString());
}

void engineChoose(Engine* engine, const char* paths)
{
    RefPtr chooser = std::exchange(engine->chooser, nullptr);
    if (!chooser)
        return;
    if (!paths) {
        if (engine->trace)
            fprintf(stderr, "voyager: file chooser cancelled\n");
        chooser->cancelFileChoosing();
        return;
    }
    Vector<String> chosen;
    for (auto line : String::fromUTF8(paths).split('\n')) {
        if (!line.isEmpty())
            chosen.append(line);
    }
    fprintf(stderr, "voyager: chose %zu files\n", chosen.size());
    chooser->chooseFiles(chosen);
    if (engine->shown)
        engine->shown->dirty = true;
}

void engineSetDownloads(Engine*, const char* directory)
{
    MlxDownloads::setDirectory(String::fromUTF8(directory));
}

int engineDownloadCount(Engine*)
{
    return MlxDownloads::list().size();
}

char* engineDownload(Engine*, int index, uint64_t* received, uint64_t* expected, int* state)
{
    auto& list = MlxDownloads::list();
    if (index < 0 || static_cast<size_t>(index) >= list.size())
        return nullptr;
    auto& download = list[index];
    *received = download.received;
    *expected = download.expected;
    *state = download.state == MlxDownloads::State::Running ? 0 : download.state == MlxDownloads::State::Done ? 1 : 2;
    return copied(download.path.isEmpty() ? download.name : FileSystem::pathFileName(download.path));
}

} // namespace

// ---------------------------------------------------------------------------
// The internal pages (voyager:)

// Voyager's version and window system, as its user agent has them.
constexpr auto voyagerVersion = "0.1"_s;
constexpr auto windowSystem = "Wayland"_s;

// The facts voyager://version shows, label and value, in its order (the
// about page takes "Voyager" and "Engine" of them).
Vector<std::pair<CString, CString>>& facts()
{
    static NeverDestroyed<Vector<std::pair<CString, CString>>> facts;
    return facts;
}

void addFact(ASCIILiteral label, const String& value)
{
    facts().append({ CString(label.characters()), value.utf8() });
}

// When the program's file was made (its link: the build's date), in UTC.
String buildDate()
{
    struct stat status;
    if (stat("/proc/self/exe", &status))
        return "unknown"_s;
    struct tm time;
    gmtime_r(&status.st_mtime, &time);
    char text[64];
    strftime(text, sizeof(text), "%Y-%m-%d %H:%M UTC", &time);
    return String::fromUTF8(text);
}

void gatherFacts(int argc, char** argv)
{
    String engineName = mlxEngine();
    String engineVersion = engineName.startsWith("WebKit "_s) ? engineName.substring(7) : engineName;
    addFact("Voyager"_s, voyagerVersion);
    addFact("Engine"_s, engineName);
    addFact("JavaScriptCore"_s, makeString(engineVersion, " (WebKit's, with the engine)"_s));
    addFact("Skia"_s, makeString("milestone "_s, SK_MILESTONE));
    addFact("mlx-unicode"_s, makeString("ICU "_s, unsafeSpan(U_ICU_VERSION), " API, in Mlx (std.unicode)"_s));
    addFact("mlx-harfbuzz"_s, makeString("HarfBuzz "_s, unsafeSpan(HB_VERSION_STRING), " API, in Mlx (std.shaping)"_s));
    addFact("mlx-zlib"_s, makeString("zlib "_s, unsafeSpan(ZLIB_VERSION), " API, in Mlx (std.flate)"_s));
    addFact("mlx-sqlite3"_s, makeString("SQLite "_s, unsafeSpan(SQLITE_VERSION), " API, in Mlx (std.sql)"_s));
    addFact("mlx-xml2"_s, makeString("libxml2 "_s, unsafeSpan(LIBXML_DOTTED_VERSION), " API, in Mlx (std.xml)"_s));
    String epoch = String::fromUTF8(VOYAGER_CODEGEN_EPOCH);
    addFact("Toolchain"_s, makeString("mlx4 (Mlx), mlx-cc (C and C++"_s, epoch.isEmpty() ? ""_s : ", code generation epoch "_s, epoch, "), mlx-ld; mlxlibc, mlxlibcpp"_s));
    String commit = String::fromUTF8(VOYAGER_GIT_COMMIT);
    addFact("Git commit"_s, commit.isEmpty() ? "not recorded by the build"_s : commit);
    addFact("Built"_s, buildDate());
    addFact("User agent"_s, mlxUserAgent("Voyager"_s, voyagerVersion, windowSystem));
    struct utsname name;
    if (!uname(&name)) {
        addFact("OS"_s, makeString(unsafeSpan(name.sysname), ' ', unsafeSpan(name.release)));
        addFact("Architecture"_s, String::fromUTF8(name.machine));
    }
    addFact("Window system"_s, makeString(windowSystem, " (std.ui.host)"_s));
    StringBuilder line;
    for (int i = 0; i < argc; ++i)
        line.append(i ? " "_s : ""_s, String::fromUTF8(argv[i]));
    addFact("Command line"_s, line.toString());
    char executable[4096];
    ssize_t length = readlink("/proc/self/exe", executable, sizeof(executable) - 1);
    if (length > 0)
        executable[length] = 0;
    addFact("Executable"_s, length > 0 ? String::fromUTF8(executable) : "unknown"_s);
    const char* configHome = getenv("XDG_CONFIG_HOME");
    const char* home = getenv("HOME");
    String settings = configHome && *configHome ? String::fromUTF8(configHome) : makeString(String::fromUTF8(home ? home : ""), "/.config"_s);
    addFact("Settings"_s, makeString(settings, "/mlx/appearance.conf (the desktop's colour scheme)"_s));
    addFact("Profile"_s, makeString(MlxProfile::dataDirectory(), " (local storage, IndexedDB, cookies)"_s));
    addFact("Cache"_s, makeString(MlxProfile::cacheDirectory(), " (the HTTP cache)"_s));
}

// Voyager's directory of a user's XDG directory (VARIABLE, else
// $HOME/FALLBACK, as the XDG Base Directory Specification has it: a
// relative path is ignored).
static String userDirectory(const char* variable, ASCIILiteral fallback)
{
    const char* value = getenv(variable);
    if (value && value[0] == '/')
        return makeString(String::fromUTF8(value), "/voyager"_s);
    const char* home = getenv("HOME");
    if (!home || home[0] != '/')
        return String();
    return makeString(String::fromUTF8(home), fallback, "/voyager"_s);
}

extern "C" void voyager_page(const char* url, const char* const* facts, size_t count, void (*deliver)(void*, uint32_t, const uint8_t*, size_t), void* context);

// A request of voyager:: the icon from the build, every other page made by
// pages.mlx.
void answerInternal(MlxSchemeTask& task)
{
    const URL& url = task.url();
    if (equalLettersIgnoringASCIICase(url.host(), "about"_s) && url.path() == "/icon.png"_s) {
        task.respond(200, "image/png"_s, String(), Vector<uint8_t>(std::span<const uint8_t>(voyagerIcon, sizeof(voyagerIcon))));
        return;
    }
    // (voyager://downloads: the downloads, each its name and what it is.)
    Vector<std::pair<CString, CString>> downloadFacts;
    bool downloadsPage = equalLettersIgnoringASCIICase(url.host(), "downloads"_s);
    if (downloadsPage) {
        downloadFacts.append({ CString("Directory"), MlxDownloads::directory().utf8() });
        for (auto& download : MlxDownloads::list()) {
            String name = download.path.isEmpty() ? download.name : FileSystem::pathFileName(download.path);
            String what;
            if (download.state == MlxDownloads::State::Done)
                what = makeString("done\t"_s, download.received, "\t"_s, download.path, '\t', download.url);
            else if (download.state == MlxDownloads::State::Failed)
                what = makeString("failed\t"_s, download.received, "\t"_s, download.error, '\t', download.url);
            else
                what = makeString("running\t"_s, download.received, '/', download.expected, "\t\t"_s, download.url);
            downloadFacts.append({ name.utf8(), what.utf8() });
        }
    }
    auto& shownFacts = downloadsPage ? downloadFacts : facts();
    Vector<const char*> pairs;
    for (auto& fact : shownFacts) {
        pairs.append(fact.first.data());
        pairs.append(fact.second.data());
    }
    auto target = url.string().utf8();
    voyager_page(target.data(), pairs.span().data(), shownFacts.size(), [](void* context, uint32_t status, const uint8_t* bytes, size_t length) {
        static_cast<MlxSchemeTask*>(context)->respond(status, "text/html"_s, "utf-8"_s, Vector<uint8_t>(std::span<const uint8_t>(bytes, length)));
    }, &task);
    if (!task.answered())
        task.fail("No memory for the page"_s);
}

// (Its result is the tag of voyager.mlx's error union: 0 when the window
// was closed, another value when there was none or its connection failed.)
extern "C" long voyager_run(const VoyagerEngine*, int argc, char** arguments);
extern char** environ;

static int usage()
{
    fprintf(stderr, "usage: voyager [--font FILE]... [--size WIDTHxHEIGHT] [--profile DIR] FILE|URL\n");
    return 2;
}

int main(int argc, char** argv)
{
    IntSize size(800, 600);
    Vector<const char*> fonts;
    const char* target = nullptr;
    const char* profile = nullptr;
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--font") && i + 1 < argc)
            fonts.append(argv[++i]);
        else if (!strcmp(argv[i], "--profile") && i + 1 < argc)
            profile = argv[++i];
        else if (!strcmp(argv[i], "--size") && i + 1 < argc) {
            int width = 0;
            int height = 0;
            if (sscanf(argv[++i], "%dx%d", &width, &height) != 2 || width <= 0 || height <= 0)
                return usage();
            size = IntSize(width, height);
        } else if (argv[i][0] == '-' || target)
            return usage();
        else
            target = argv[i];
    }
    if (!target)
        return usage();

    JSC::initialize();
    WTF::initializeMainThread();
    initializeCommonAtomStrings();
    MlxPlatformStrategies::initialize();
    for (auto* font : fonts) {
        if (!mlxweb_font_add_file(font)) {
            fprintf(stderr, "voyager: no font in %s\n", font);
            return 2;
        }
    }

    static Engine engine;
    engine.size = size;
    engine.trace = getenv("VOYAGER_TRACE");
    // The user agent: WebKit's compatibility string, then Voyager's name
    // and version with the engine and the window system (std.ui's Linux
    // host shows windows on Wayland).
    MlxFrameLoaderClient::setApplicationNameForUserAgent("Voyager"_s, voyagerVersion, windowSystem);
    if (engine.trace)
        fprintf(stderr, "voyager: user agent %s\n", mlxUserAgent("Voyager"_s, voyagerVersion, windowSystem).utf8().data());
    // The profile: --profile's directory (the cache in DIR/cache), else
    // the XDG directories' voyager/ (MlxProfile.h).
    if (profile) {
        String directory = String::fromUTF8(profile);
        char cwd[4096];
        if (!directory.startsWith('/') && getcwd(cwd, sizeof(cwd)))
            directory = FileSystem::pathByAppendingComponent(String::fromUTF8(cwd), directory);
        MlxProfile::open(directory, FileSystem::pathByAppendingComponent(directory, "cache"_s));
    } else
        MlxProfile::open(userDirectory("XDG_DATA_HOME", "/.local/share"_s), userDirectory("XDG_CACHE_HOME", "/.cache"_s));
    if (engine.trace)
        fprintf(stderr, "voyager: profile %s, cache %s\n", MlxProfile::dataDirectory().utf8().data(), MlxProfile::cacheDirectory().utf8().data());
    // The internal pages.
    gatherFacts(argc, argv);
    MlxSchemeHandler::registerScheme("voyager"_s, answerInternal);
    // Each download's end, on standard error (tools/check_voyager.sh).
    MlxDownloads::setObserver([](const MlxDownloads::Download* changed) {
        if (!changed)
            return;
        auto& download = *changed;
        if (download.state == MlxDownloads::State::Done)
            fprintf(stderr, "voyager: download done %s (%llu bytes)\n", download.path.utf8().data(), static_cast<unsigned long long>(download.received));
        else if (download.state == MlxDownloads::State::Failed)
            fprintf(stderr, "voyager: download failed %s: %s\n", download.name.utf8().data(), download.error.utf8().data());
        else if (!download.received && download.path.isEmpty())
            fprintf(stderr, "voyager: download of %s as %s\n", download.url.utf8().data(), download.name.utf8().data());
    });
    // The first tab, opened by the engine (the window shows it).
    Tab* first = openTab(engine, true);
    if (!first) {
        fprintf(stderr, "voyager: the page has no main frame\n");
        return 1;
    }
    if (!loadInto(first, target)) {
        fprintf(stderr, "voyager: %s is no URL\n", target);
        return 2;
    }

    VoyagerEngine table {
        &engine, engineOpen, engineClose, engineSelect, engineOpened, engineLoad, engineResize, engineCycle, enginePaint,
        enginePointer, engineWheel, engineKey, engineSelectedText, engineInsertText, engineTitle, engineUrl, engineRelease,
        engineReload, engineStop, engineLoading, engineSetDark, engineForcedDark,
        engineChooser, engineChoose, engineSetDownloads, engineDownloadCount, engineDownload
    };
    // main's arguments, then the environment with MLX_CANVAS=cpu (the
    // window drawn into wl_shm buffers on the CPU, which the engine
    // paints), as voyager_run reads them.
    Vector<char*> arguments;
    for (int i = 0; i < argc; ++i)
        arguments.append(argv[i]);
    arguments.append(nullptr);
    for (char** entry = environ; *entry; ++entry) {
        if (strncmp(*entry, "MLX_CANVAS=", 11))
            arguments.append(*entry);
    }
    static char canvas[] = "MLX_CANVAS=cpu";
    arguments.append(canvas);
    arguments.append(nullptr);
    long failed = voyager_run(&table, argc, arguments.mutableSpan().data());
    // (What the profile has pending written: local storage's changes.)
    MlxProfile::close();
    if (failed) {
        fprintf(stderr, "voyager: no window, or its connection ended (is a Wayland compositor running? WAYLAND_DISPLAY)\n");
        return 1;
    }
    return 0;
}
