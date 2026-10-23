// Voyager's engine (PORT.md, M5): WebCore in Voyager's process, as the
// port's single-process embedding has it. This is the program's main: it
// makes a Page of the port's clients (MlxFrameLoaderClient, the port's
// platform strategies; a chrome client that notes what needs painting;
// WebCore's empty clients for the rest) and hands the window to
// voyager_run (projects/voyager/voyager.mlx, libvoyager.so), which owns the
// Wayland window (std.ui.host: wl_shm buffers, keys, clipboard) and calls
// back through the table of functions below: load a URL, run WebCore's
// pending work (WTF's generic RunLoop, one cycle), paint the view into the
// window's pixels (Skia raster through GraphicsContextSkia), and the
// pointer's, the wheel's and the keyboard's events (WebCore's
// EventHandler; text typed into an editable element through the Editor,
// editing keys as its commands), the selection's text and pasting.
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
#include "CookieJar.h"
#include "port/embed/MlxNetwork.h"
#include "port/embed/MlxPlatformStrategies.h"
#include <JavaScriptCore/InitializeThreading.h>
#include <pal/SessionID.h>
#include <skia/core/SkCanvas.h>
#include <skia/core/SkSurface.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wtf/FileSystem.h>
#include <wtf/MainThread.h>
#include <wtf/RunLoop.h>
#include <wtf/text/CString.h>

using namespace WebCore;

namespace {

// What Voyager's window asks of the engine: the functions of
// VoyagerEngine (voyager.mlx declares the same table).
struct Engine;

struct VoyagerEngine {
    Engine* engine;
    int (*load)(Engine*, const char* url);
    void (*resize)(Engine*, int width, int height);
    int (*cycle)(Engine*);
    void (*paint)(Engine*, uint32_t* pixels, int width, int height, int stride);
    void (*pointer)(Engine*, int kind, int x, int y, int button, unsigned modifiers);
    void (*wheel)(Engine*, int x, int y, int deltaX, int deltaY);
    void (*key)(Engine*, int down, unsigned code, const char* text, unsigned modifiers);
    char* (*selectedText)(Engine*);
    void (*insertText)(Engine*, const char* text);
    char* (*title)(Engine*);
    char* (*url)(Engine*);
    void (*release)(char*);
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

// The chrome client: what WebCore repaints marks the view dirty (the
// window paints it again on its next frame).
class VoyagerChromeClient final : public EmptyChromeClient {
public:
    explicit VoyagerChromeClient(bool& dirty)
        : m_dirty(dirty)
    {
    }

private:
    void invalidateContentsAndRootView(const IntRect&) final { m_dirty = true; }

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

struct Engine {
    RefPtr<Page> page;
    RefPtr<LocalFrame> frame;
    MlxFrameLoaderClient* client { nullptr };
    IntSize size;
    bool dirty { true };
    int clickCount { 0 };
    MonotonicTime lastPress;
    IntPoint lastPressPosition;
    IntPoint pointerPosition;
    MouseButton pressedButton { MouseButton::None };
    MonotonicTime lastUpdate;
    // VOYAGER_TRACE in the environment: each event on standard error.
    bool trace { false };
};

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
    if (argument.startsWith("file:"_s) || argument.startsWith("data:"_s) || argument.startsWith("http:"_s) || argument.startsWith("https:"_s))
        return URL { argument };
    String path = argument;
    char directory[4096];
    if (!argument.startsWith('/') && getcwd(directory, sizeof(directory)))
        path = FileSystem::pathByAppendingComponent(String::fromUTF8(directory), argument);
    return URL::fileURLWithFileSystemPath(path);
}

int engineLoad(Engine* engine, const char* target)
{
    URL url = urlOf(target);
    if (!url.isValid())
        return 0;
    engine->frame->loader().load(FrameLoadRequest(*engine->frame, ResourceRequest(URL { url })));
    engine->dirty = true;
    return 1;
}

void engineResize(Engine* engine, int width, int height)
{
    if (width <= 0 || height <= 0 || IntSize(width, height) == engine->size)
        return;
    engine->size = IntSize(width, height);
    if (RefPtr view = engine->frame->view())
        view->resize(engine->size);
    engine->dirty = true;
}

// One turn of WebCore's loop (timers, loads, tasks); 1 when the view is
// to be painted again.
int engineCycle(Engine* engine)
{
    RunLoop::cycle();
    // The page's rendering update (WebKit's display link: scroll and
    // resize events, animation frames, intersection observers), at most
    // every 30 ms.
    auto now = MonotonicTime::now();
    if (now - engine->lastUpdate >= 30_ms) {
        engine->lastUpdate = now;
        engine->page->isolatedUpdateRendering();
    }
    if (RefPtr view = engine->frame->view()) {
        if (view->needsLayout())
            engine->dirty = true;
    }
    return engine->dirty ? 1 : 0;
}

void enginePaint(Engine* engine, uint32_t* pixels, int width, int height, int stride)
{
    engine->dirty = false;
    RefPtr view = engine->frame->view();
    if (!view)
        return;
    view->updateLayoutAndStyleIfNeededRecursive();
    auto info = SkImageInfo::MakeN32Premul(width, height);
    auto surface = SkSurfaces::WrapPixels(info, pixels, static_cast<size_t>(stride) * sizeof(uint32_t));
    if (!surface)
        return;
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
    IntPoint position(x, y);
    engine->pointerPosition = position;
    if (engine->trace && kind != pointerMotion)
        fprintf(stderr, "voyager: pointer %s %d at %d,%d\n", kind == pointerPress ? "press" : "release", button, x, y);
    auto& handler = engine->frame->eventHandler();
    auto now = MonotonicTime::now();
    if (kind == pointerPress) {
        auto pressed = buttonOf(button);
        // A press near the last one, soon after: a double (triple) click.
        if (pressed == engine->pressedButton && now - engine->lastPress < 400_ms && (position - engine->lastPressPosition).diagonalLengthSquared() <= 25)
            engine->clickCount++;
        else
            engine->clickCount = 1;
        engine->lastPress = now;
        engine->lastPressPosition = position;
        engine->pressedButton = pressed;
        handler.handleMousePressEvent(PlatformMouseEvent(position, position, pressed, PlatformEvent::Type::MousePressed, engine->clickCount, modifiersOf(modifiers), now, 0, SyntheticClickType::NoTap, MouseEventInputSource::UserDriven));
    } else if (kind == pointerRelease)
        handler.handleMouseReleaseEvent(PlatformMouseEvent(position, position, buttonOf(button), PlatformEvent::Type::MouseReleased, engine->clickCount, modifiersOf(modifiers), now, 0, SyntheticClickType::NoTap, MouseEventInputSource::UserDriven));
    if (engine->trace && kind == pointerRelease) {
        auto& selection = engine->frame->selection().selection();
        auto result = engine->frame->eventHandler().hitTestResultAtPoint(LayoutPoint(position), { HitTestRequest::Type::ReadOnly, HitTestRequest::Type::Active });
        RefPtr node = result.innerNode();
        fprintf(stderr, "voyager: selection none %d caret %d editable %d; hit %s\n", selection.isNone(), selection.isCaret(), !!selection.rootEditableElement(), node ? node->nodeName().utf8().data() : "nothing");
    }
    if (kind == pointerMotion)
        handler.handleMouseMoveEvent(PlatformMouseEvent(position, position, button ? buttonOf(button) : MouseButton::None, PlatformEvent::Type::MouseMoved, 0, modifiersOf(modifiers), now, 0, SyntheticClickType::NoTap, MouseEventInputSource::UserDriven));
    engine->dirty = true;
}

void engineWheel(Engine* engine, int x, int y, int deltaX, int deltaY)
{
    if (engine->trace)
        fprintf(stderr, "voyager: wheel %d,%d at %d,%d\n", deltaX, deltaY, x, y);
    IntPoint position(x, y);
    // (WebCore's wheel deltas point the other way: positive scrolls up.)
    PlatformWheelEvent event(position, position, -deltaX, -deltaY, -deltaX / 40.0f, -deltaY / 40.0f, PlatformWheelEventGranularity::ScrollByPixelWheelEvent, false, false, false, false);
    engine->frame->eventHandler().handleWheelEvent(event, { WheelEventProcessingSteps::SynchronousScrolling, WheelEventProcessingSteps::BlockingDOMEventDispatch });
    engine->dirty = true;
}

// A key pressed (DOWN 1) or released (0): CODE is its Linux key code, TEXT
// what it types (UTF-8, empty for none). The page sees it first (keydown,
// keypress, keyup); what it leaves undone, the engine does: the text
// typed into the focused editable element, an editing key's command.
void engineKey(Engine* engine, int down, unsigned code, const char* text, unsigned modifiers)
{
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
    bool handled = engine->frame->eventHandler().keyEvent(event);
    engine->dirty = true;
    if (engine->trace && down) {
        RefPtr focusedFrame = engine->page->focusController().focusedOrMainFrame();
        RefPtr document = focusedFrame ? focusedFrame->document() : nullptr;
        RefPtr element = document ? document->focusedElement() : nullptr;
        fprintf(stderr, "voyager: key handled %d, focus %s, editable %d\n", handled, element ? element->localName().string().utf8().data() : "none", focusedFrame ? focusedFrame->editor().canEdit() : -1);
    }
    if (!down || handled)
        return;
    RefPtr focused = engine->page->focusController().focusedOrMainFrame();
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
    RefPtr focused = engine->page->focusController().focusedOrMainFrame();
    String text = focused ? focused->editor().selectedText() : String();
    return text.isEmpty() ? nullptr : copied(text);
}

void engineInsertText(Engine* engine, const char* text)
{
    RefPtr focused = engine->page->focusController().focusedOrMainFrame();
    if (!focused || !text)
        return;
    auto& editor = focused->editor();
    if (editor.canEdit())
        editor.insertText(String::fromUTF8(text), nullptr);
    engine->dirty = true;
}

char* engineTitle(Engine* engine)
{
    RefPtr document = engine->frame->document();
    return copied(document ? document->title() : String());
}

char* engineUrl(Engine* engine)
{
    RefPtr document = engine->frame->document();
    return copied(document ? document->url().string() : String());
}

void engineRelease(char* string)
{
    free(string);
}

} // namespace

// (Its result is the tag of voyager.mlx's error union: 0 when the window
// was closed, another value when there was none or its connection failed.)
extern "C" long voyager_run(const VoyagerEngine*, int argc, char** arguments);
extern char** environ;

static int usage()
{
    fprintf(stderr, "usage: voyager [--font FILE]... [--size WIDTHxHEIGHT] FILE|URL\n");
    return 2;
}

int main(int argc, char** argv)
{
    IntSize size(800, 600);
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
    auto configuration = pageConfigurationWithEmptyClients(std::nullopt, PAL::SessionID::defaultSessionID());
    configuration.chromeClient = makeUniqueRef<VoyagerChromeClient>(engine.dirty);
    configuration.editorClient = makeUniqueRef<VoyagerEditorClient>();
    configuration.cookieJar = MlxNetwork::cookieJar();
    configuration.mainFrameCreationParameters = PageConfiguration::LocalMainFrameCreationParameters {
        CompletionHandler<UniqueRef<LocalFrameLoaderClient>(LocalFrame&, FrameLoader&)> { [size](auto&, auto& frameLoader) {
            auto client = makeUniqueRefWithoutRefCountedCheck<MlxFrameLoaderClient>(frameLoader, size);
            engine.client = client.ptr();
            return client;
        } },
        SandboxFlags { },
        ReferrerPolicy::EmptyString
    };
    engine.page = Page::create(WTF::move(configuration));
    auto& settings = engine.page->settings();
    settings.setScriptEnabled(true);
    settings.setAcceleratedCompositingEnabled(false);
    settings.setLoadsImagesAutomatically(true);
    settings.setAllowFileAccessFromFileURLs(true);
    settings.setUsesBackForwardCache(false);
    engine.frame = engine.page->localMainFrame();
    if (!engine.frame || !engine.client) {
        fprintf(stderr, "voyager: the page has no main frame\n");
        return 1;
    }
    engine.frame->init();
    engine.page->focusController().setActive(true);
    engine.page->focusController().setFocused(true);
    if (!engineLoad(&engine, target)) {
        fprintf(stderr, "voyager: %s is no URL\n", target);
        return 2;
    }

    VoyagerEngine table {
        &engine, engineLoad, engineResize, engineCycle, enginePaint, enginePointer, engineWheel, engineKey,
        engineSelectedText, engineInsertText, engineTitle, engineUrl, engineRelease
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
    if (voyager_run(&table, argc, arguments.mutableSpan().data())) {
        fprintf(stderr, "voyager: no window, or its connection ended (is a Wayland compositor running? WAYLAND_DISPLAY)\n");
        return 1;
    }
    return 0;
}
