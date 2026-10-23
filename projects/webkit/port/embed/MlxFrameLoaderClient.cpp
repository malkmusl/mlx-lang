// The Mlx port's frame loader client (MlxFrameLoaderClient.h).
#include "config.h"
#include "MlxFrameLoaderClient.h"

#include "Chrome.h"
#include "DOMWrapperWorld.h"
#include "Document.h"
#include "FrameTreeSyncData.h"
#include "DocumentLoader.h"
#include "FormState.h"
#include "FrameLoader.h"
#include "FrameNetworkingContext.h"
#include "HTMLFrameOwnerElement.h"
#include "HTMLPlugInElement.h"
#include "HistoryItem.h"
#include "LocalFrame.h"
#include "LocalFrameView.h"
#include "MIMETypeRegistry.h"
#include "NavigationAction.h"
#include "Page.h"
#include "WindowFeatures.h"
#include "ResourceError.h"
#include "ResourceRequest.h"
#include "ResourceResponse.h"
#include "SharedBuffer.h"
#include "SubstituteData.h"
#include "UserAgent.h"
#include "UserAgentMlx.h"
#include "port/embed/MlxDownloads.h"
#include <wtf/NeverDestroyed.h>
#include "Widget.h"
#include <stdio.h>
#include <stdlib.h>
#include <wtf/text/CString.h>

namespace WebCore {

// MLX_WEBCORE_TRACE=1 in the environment: the client's calls on standard
// error, a load's steps as WebCore takes them.
static bool tracing()
{
    static bool on = !!getenv("MLX_WEBCORE_TRACE");
    return on;
}

// MLX_WEBCORE_TRACE: what the client is told, on standard error.
#define MLX_TRACE(...) do { if (tracing()) { fprintf(stderr, "frame loader client: " __VA_ARGS__); fputc('\n', stderr); } } while (0)


// The frame's networking context: the frame, and no storage session of
// its own yet (cookies come with the network, M6).
class MlxFrameNetworkingContext final : public FrameNetworkingContext {
public:
    static Ref<MlxFrameNetworkingContext> create(LocalFrame& frame) { return adoptRef(*new MlxFrameNetworkingContext(frame)); }

private:
    explicit MlxFrameNetworkingContext(LocalFrame& frame)
        : FrameNetworkingContext(&frame)
    {
    }

    NetworkStorageSession* storageSession() const final { return nullptr; }
};

MlxFrameLoaderClient::MlxFrameLoaderClient(FrameLoader& frameLoader, IntSize viewSize)
    : LocalFrameLoaderClient(frameLoader)
    , m_viewSize(viewSize)
{
}

MlxFrameLoaderClient::~MlxFrameLoaderClient() = default;

void MlxFrameLoaderClient::loadFinished(bool succeeded)
{
    MLX_TRACE("load finished (%s)", succeeded ? "succeeded" : "failed");
    if (m_loadFinished)
        m_loadFinished(succeeded);
}

Ref<DocumentLoader> MlxFrameLoaderClient::createDocumentLoader(ResourceRequest&& request, SubstituteData&& substituteData, ResourceRequest&& originalRequest)
{
    return DocumentLoader::create(WTF::move(request), WTF::move(substituteData), WTF::move(originalRequest));
}

Ref<DocumentLoader> MlxFrameLoaderClient::createDocumentLoader(ResourceRequest&& request, SubstituteData&& substituteData)
{
    return DocumentLoader::create(WTF::move(request), WTF::move(substituteData), { });
}

bool MlxFrameLoaderClient::hasWebView() const { return true; }
void MlxFrameLoaderClient::makeRepresentation(DocumentLoader*) { }
void MlxFrameLoaderClient::forceLayoutForNonHTML() { }
void MlxFrameLoaderClient::setCopiesOnScroll() { }
void MlxFrameLoaderClient::detachedFromParent2() { }
void MlxFrameLoaderClient::detachedFromParent3() { }
// (The frame's load stops here; its body goes on into the download's
// file, MlxDownloads::convert.)
void MlxFrameLoaderClient::convertMainResourceLoadToDownload(DocumentLoader* documentLoader, const ResourceRequest& request, const ResourceResponse& response)
{
    m_downloadStarted = true;
    MlxDownloads::convert(documentLoader ? documentLoader->mainResourceLoader() : nullptr, request, response, std::exchange(m_downloadName, String()));
}

void MlxFrameLoaderClient::assignIdentifierToInitialRequest(ResourceLoaderIdentifier, DocumentLoader*, const ResourceRequest&) { MLX_TRACE("assignIdentifierToInitialRequest"); }
bool MlxFrameLoaderClient::shouldUseCredentialStorage(DocumentLoader*, ResourceLoaderIdentifier) { return false; }
void MlxFrameLoaderClient::dispatchWillSendRequest(DocumentLoader*, ResourceLoaderIdentifier, ResourceRequest&, const ResourceResponse&) { MLX_TRACE("dispatchWillSendRequest"); }
void MlxFrameLoaderClient::dispatchDidReceiveAuthenticationChallenge(DocumentLoader*, ResourceLoaderIdentifier, const AuthenticationChallenge&) { }
#if USE(PROTECTION_SPACE_AUTH_CALLBACK)
bool MlxFrameLoaderClient::canAuthenticateAgainstProtectionSpace(DocumentLoader*, ResourceLoaderIdentifier, const ProtectionSpace&) { return false; }
#endif
void MlxFrameLoaderClient::dispatchDidReceiveResponse(DocumentLoader*, ResourceLoaderIdentifier, const ResourceResponse&) { MLX_TRACE("dispatchDidReceiveResponse"); }
void MlxFrameLoaderClient::dispatchDidReceiveContentLength(DocumentLoader*, ResourceLoaderIdentifier, int) { MLX_TRACE("dispatchDidReceiveContentLength"); }
void MlxFrameLoaderClient::dispatchDidFinishLoading(DocumentLoader*, ResourceLoaderIdentifier) { }
void MlxFrameLoaderClient::dispatchDidFailLoading(DocumentLoader*, ResourceLoaderIdentifier, const ResourceError&) { }
bool MlxFrameLoaderClient::dispatchDidLoadResourceFromMemoryCache(DocumentLoader*, const ResourceRequest&, const ResourceResponse&, int) { return false; }

void MlxFrameLoaderClient::dispatchDidDispatchOnloadEvents() { }
void MlxFrameLoaderClient::dispatchDidReceiveServerRedirectForProvisionalLoad() { }
void MlxFrameLoaderClient::dispatchDidCancelClientRedirect() { }
void MlxFrameLoaderClient::dispatchWillPerformClientRedirect(const URL&, double, WallTime, LockBackForwardList) { }
void MlxFrameLoaderClient::dispatchDidChangeLocationWithinPage() { }
void MlxFrameLoaderClient::dispatchDidPushStateWithinPage() { }
void MlxFrameLoaderClient::dispatchDidReplaceStateWithinPage() { }
void MlxFrameLoaderClient::dispatchDidPopStateWithinPage() { }
void MlxFrameLoaderClient::dispatchWillClose() { MLX_TRACE("dispatchWillClose"); }
void MlxFrameLoaderClient::dispatchDidStartProvisionalLoad()
{
    MLX_TRACE("provisional load started");
}
void MlxFrameLoaderClient::dispatchDidReceiveTitle(const StringWithDirection&) { }
void MlxFrameLoaderClient::dispatchDidCommitLoad(std::optional<HasInsecureContent>, std::optional<UsedLegacyTLS>, std::optional<WasPrivateRelayed>)
{
    MLX_TRACE("committed %s", m_loader->frame().document() ? m_loader->frame().document()->url().string().utf8().data() : "(no document)");
}

void MlxFrameLoaderClient::dispatchDidFailProvisionalLoad(const ResourceError& error, WillContinueLoading willContinueLoading, WillInternallyHandleFailure)
{
    MLX_TRACE("provisional load failed: %s", error.localizedDescription().utf8().data());
    // (A load turned into a download, its policy changed, is no failure of
    // the frame's: the page shown stays.)
    if (std::exchange(m_downloadStarted, false))
        return;
    if (willContinueLoading == WillContinueLoading::No)
        loadFinished(false);
}

void MlxFrameLoaderClient::dispatchDidFailLoad(const ResourceError& error)
{
    MLX_TRACE("load failed: %s", error.localizedDescription().utf8().data());
    loadFinished(false);
}

void MlxFrameLoaderClient::dispatchDidFinishDocumentLoad() { }

void MlxFrameLoaderClient::dispatchDidFinishLoad()
{
    loadFinished(true);
}

void MlxFrameLoaderClient::dispatchDidReachLayoutMilestone(OptionSet<LayoutMilestone>) { }
void MlxFrameLoaderClient::dispatchDidReachVisuallyNonEmptyState() { }

// A new page for a link's or a form's target (`target=_blank`): the one
// the page's chrome client makes for window.open too (an embedder with
// tabs gives a new tab; WebCore's empty chrome client none).
LocalFrame* MlxFrameLoaderClient::dispatchCreatePage(const NavigationAction& action, NewFrameOpenerPolicy, const String& name)
{
    Ref frame = m_loader->frame();
    RefPtr page = frame->page();
    if (!page)
        return nullptr;
    RefPtr newPage = page->chrome().createWindow(frame, name, { }, action);
    return newPage ? newPage->localMainFrame() : nullptr;
}
void MlxFrameLoaderClient::dispatchShow() { }

// What the embedder allows: every navigation of the frame itself and every
// response it can show; a new window where its chrome client makes one
// (dispatchCreatePage). Downloaded (MlxDownloads): a link with a download
// attribute, a response that is an attachment (Content-Disposition) or
// of a type no frame shows; one in error that cannot be shown is
// ignored.
void MlxFrameLoaderClient::dispatchDecidePolicyForResponse(const ResourceResponse& response, const ResourceRequest&, const String& downloadAttribute, FramePolicyFunction&& function)
{
    bool shown = canShowMIMEType(response.mimeType());
    bool download = !downloadAttribute.isNull() || response.isAttachment() || (!shown && response.httpStatusCode() < 400);
    MLX_TRACE("response %s (%s): %s", response.url().string().utf8().data(), response.mimeType().utf8().data(), download ? "downloaded" : shown ? "shown" : "ignored");
    if (download)
        m_downloadName = downloadAttribute;
    function(download ? PolicyAction::Download : shown ? PolicyAction::Use : PolicyAction::Ignore);
}

void MlxFrameLoaderClient::dispatchDecidePolicyForNewWindowAction(const NavigationAction&, const ResourceRequest& request, FormState*, const String&, std::optional<HitTestResult>&&, FramePolicyFunction&& function)
{
    MLX_TRACE("new window for %s", request.url().string().utf8().data());
    function(PolicyAction::Use);
}

// (A link with a download attribute is downloaded at once, before any
// load: WebCore's startDownload, its blob still registered.)
void MlxFrameLoaderClient::dispatchDecidePolicyForNavigationAction(const NavigationAction& action, const ResourceRequest& request, const ResourceResponse&, FormState*, const String&, std::optional<NavigationIdentifier>, std::optional<HitTestResult>&&, bool, NavigationUpgradeToHTTPSBehavior, SandboxFlags, PolicyDecisionMode, FramePolicyFunction&& function)
{
    MLX_TRACE("navigation to %s%s", request.url().string().utf8().data(), action.downloadAttribute().isNull() ? "" : " (a download)");
    function(action.downloadAttribute().isNull() ? PolicyAction::Use : PolicyAction::Download);
}

void MlxFrameLoaderClient::updateSandboxFlags(SandboxFlags) { }
void MlxFrameLoaderClient::updateOpener(std::optional<FrameIdentifier>) { }
void MlxFrameLoaderClient::setPrinting(bool, FloatSize, FloatSize, float, AdjustViewSize) { }
void MlxFrameLoaderClient::cancelPolicyCheck() { }
void MlxFrameLoaderClient::dispatchUnableToImplementPolicy(const ResourceError&) { }

void MlxFrameLoaderClient::dispatchWillSendSubmitEvent(Ref<FormState>&&) { }

void MlxFrameLoaderClient::dispatchWillSubmitForm(FormState&, URL&&, String&&, CompletionHandler<void()>&& completionHandler)
{
    completionHandler();
}

void MlxFrameLoaderClient::revertToProvisionalState(DocumentLoader*) { }
void MlxFrameLoaderClient::setMainDocumentError(DocumentLoader*, const ResourceError&) { }
void MlxFrameLoaderClient::setMainFrameDocumentReady(bool) { }
void MlxFrameLoaderClient::startDownload(const ResourceRequest& request, const String& suggestedName, FromDownloadAttribute)
{
    MlxDownloads::start(request, nullptr, suggestedName);
}
void MlxFrameLoaderClient::willChangeTitle(DocumentLoader*) { }
void MlxFrameLoaderClient::didChangeTitle(DocumentLoader*) { }
void MlxFrameLoaderClient::willReplaceMultipartContent() { }
void MlxFrameLoaderClient::didReplaceMultipartContent() { }

// The main resource's data, to the document's parser.
void MlxFrameLoaderClient::committedLoad(DocumentLoader* loader, const SharedBuffer& data)
{
    MLX_TRACE("data: %zu bytes for %p (frame %p, the frame's loader %p, provisional %p)", data.size(), loader, loader->frame(), m_loader->documentLoader(), m_loader->provisionalDocumentLoader());
    loader->commitData(data);
}

void MlxFrameLoaderClient::finishedLoading(DocumentLoader*) { }
void MlxFrameLoaderClient::loadStorageAccessQuirksIfNeeded() { }
bool MlxFrameLoaderClient::shouldFallBack(const ResourceError&) const { return false; }

bool MlxFrameLoaderClient::canHandleRequest(const ResourceRequest&) const
{
    return true;
}

bool MlxFrameLoaderClient::canShowMIMEType(const String& mimeType) const
{
    return MIMETypeRegistry::canShowMIMEType(mimeType);
}

bool MlxFrameLoaderClient::canShowMIMETypeAsHTML(const String& mimeType) const
{
    return MIMETypeRegistry::isSupportedNonImageMIMEType(mimeType);
}

bool MlxFrameLoaderClient::representationExistsForURLScheme(StringView) const { return false; }
String MlxFrameLoaderClient::generatedMIMETypeForURLScheme(StringView) const { return emptyString(); }

void MlxFrameLoaderClient::frameLoadCompleted() { }
void MlxFrameLoaderClient::restoreViewState() { }
void MlxFrameLoaderClient::provisionalLoadStarted() { MLX_TRACE("provisionalLoadStarted"); }
void MlxFrameLoaderClient::didFinishLoad() { }
void MlxFrameLoaderClient::prepareForDataSourceReplacement() { }
void MlxFrameLoaderClient::updateCachedDocumentLoader(DocumentLoader&) { }
void MlxFrameLoaderClient::setTitle(const StringWithDirection&, const URL&) { }

static String& applicationName()
{
    static NeverDestroyed<String> name;
    return name;
}

static String& applicationVersion()
{
    static NeverDestroyed<String> version;
    return version;
}

static String& windowSystem()
{
    static NeverDestroyed<String> system;
    return system;
}

void MlxFrameLoaderClient::setApplicationNameForUserAgent(const String& name, const String& version, const String& system)
{
    applicationName() = name;
    applicationVersion() = version;
    windowSystem() = system;
}

// The port's user agent (UserAgentMlx.cpp) with the embedder's part, a
// site's quirk instead when it has one (UserAgentQuirks). navigator's
// and the requests' User-Agent are both this.
String MlxFrameLoaderClient::userAgent(const URL& url) const
{
    auto forURL = standardUserAgentForURL(url);
    if (!forURL.isNull())
        return forURL;
    return mlxUserAgent(applicationName(), applicationVersion(), windowSystem());
}

void MlxFrameLoaderClient::savePlatformDataToCachedFrame(CachedFrame*) { }
void MlxFrameLoaderClient::transitionToCommittedFromCachedFrame(CachedFrame*) { }

// A view for the new document, of the embedder's size, scrolled as needed.
void MlxFrameLoaderClient::transitionToCommittedForNewPage(InitializingIframe)
{
    Ref frame = m_loader->frame();
    MLX_TRACE("new page: a view for %p", m_loader->documentLoader());
    RefPtr oldView = frame->view();
    IntSize size = !frame->isMainFrame() && oldView ? oldView->size() : m_viewSize;
    frame->createView(size, std::nullopt, { }, false, ScrollbarMode::Auto, false, ScrollbarMode::Auto, false);
}

void MlxFrameLoaderClient::didRestoreFromBackForwardCache() { }

void MlxFrameLoaderClient::updateGlobalHistory() { }
void MlxFrameLoaderClient::updateGlobalHistoryRedirectLinks() { }
ShouldGoToHistoryItem MlxFrameLoaderClient::shouldGoToHistoryItem(HistoryItem&, IsSameDocumentNavigation) const { return ShouldGoToHistoryItem::No; }
bool MlxFrameLoaderClient::supportsAsyncShouldGoToHistoryItem() const { return false; }

void MlxFrameLoaderClient::shouldGoToHistoryItemAsync(HistoryItem&, CompletionHandler<void(ShouldGoToHistoryItem)>&& completionHandler) const
{
    completionHandler(ShouldGoToHistoryItem::No);
}

void MlxFrameLoaderClient::dispatchGoToBackForwardItemAtIndex(int) { }
void MlxFrameLoaderClient::dispatchEnqueueHistoryTraversalDelta(int) { }
void MlxFrameLoaderClient::saveViewStateToItem(HistoryItem&) { }
bool MlxFrameLoaderClient::canCachePage() const { return false; }

// A frame of an <iframe> (or <frame>, <object>) of the frame's document:
// a subframe of the page with a client of its own, as WebKit2's
// WebFrame::createSubframe makes one (its sandbox flags the owner's and
// the parent's, its referrer policy the owner's, else the top document's).
RefPtr<LocalFrame> MlxFrameLoaderClient::createFrame(const AtomString& name, HTMLFrameOwnerElement& ownerElement)
{
    Ref parent = m_loader->frame();
    RefPtr page = parent->page();
    if (!page)
        return nullptr;
    auto sandboxFlags = ownerElement.sandboxFlags();
    sandboxFlags.add(parent->effectiveSandboxFlags());
    auto referrerPolicy = ownerElement.referrerPolicy();
    if (RefPtr topDocument = page->localTopDocument(); referrerPolicy == ReferrerPolicy::EmptyString && topDocument)
        referrerPolicy = topDocument->referrerPolicy();
    IntSize size = m_viewSize;
    Ref frame = LocalFrame::createSubframe(*page, [size](auto&, auto& frameLoader) {
        return makeUniqueRefWithoutRefCountedCheck<MlxFrameLoaderClient>(frameLoader, size);
    }, generateFrameIdentifier(), sandboxFlags, referrerPolicy, ownerElement, FrameTreeSyncData::create());
    frame->tree().setSpecifiedName(name);
    frame->init();
    // (Its creation may have run a script that took it out of the page.)
    if (!frame->page())
        return nullptr;
    MLX_TRACE("subframe %s", name.string().utf8().data());
    return frame;
}
RefPtr<Widget> MlxFrameLoaderClient::createPlugin(HTMLPlugInElement&, const URL&, const Vector<AtomString>&, const Vector<AtomString>&, const String&, bool) { return nullptr; }
void MlxFrameLoaderClient::redirectDataToPlugin(Widget&) { }

// What an <object> or <embed> shows: an image, a frame for what a frame
// shows; no plug-ins.
ObjectContentType MlxFrameLoaderClient::objectContentType(const URL& url, const String& mimeTypeIn)
{
    String mimeType = mimeTypeIn;
    if (mimeType.isEmpty()) {
        auto path = url.path();
        auto dot = path.reverseFind('.');
        if (dot == notFound)
            return ObjectContentType::Frame;
        mimeType = MIMETypeRegistry::mimeTypeForExtension(path.substring(dot + 1));
        if (mimeType.isEmpty())
            return ObjectContentType::Frame;
    }
    if (MIMETypeRegistry::isSupportedImageMIMEType(mimeType))
        return ObjectContentType::Image;
    if (MIMETypeRegistry::isSupportedNonImageMIMEType(mimeType))
        return ObjectContentType::Frame;
    return ObjectContentType::None;
}

AtomString MlxFrameLoaderClient::overrideMediaType() const { return nullAtom(); }
static Function<void(LocalFrame&)>& windowObjectClearedCallback()
{
    static NeverDestroyed<Function<void(LocalFrame&)>> callback;
    return callback;
}

void MlxFrameLoaderClient::setWindowObjectClearedCallback(Function<void(LocalFrame&)>&& callback)
{
    windowObjectClearedCallback() = WTF::move(callback);
}

void MlxFrameLoaderClient::dispatchDidClearWindowObjectInWorld(DOMWrapperWorld& world)
{
    if (!world.isNormal() || !windowObjectClearedCallback())
        return;
    windowObjectClearedCallback()(m_loader->frame());
}

Ref<FrameNetworkingContext> MlxFrameLoaderClient::createNetworkingContext()
{
    return MlxFrameNetworkingContext::create(m_loader->frame());
}

void MlxFrameLoaderClient::prefetchDNS(const String&) { }

void MlxFrameLoaderClient::sendH2Ping(const URL& url, CompletionHandler<void(Expected<Seconds, ResourceError>&&)>&& completionHandler)
{
    completionHandler(makeUnexpected(ResourceError(errorDomainWebKitInternal, 0, url, "HTTP/2 is not supported"_s)));
}

void MlxFrameLoaderClient::dispatchLoadEventToOwnerElementInAnotherProcess() { }

RefPtr<HistoryItem> MlxFrameLoaderClient::createHistoryItemTree(bool, BackForwardItemIdentifier) const { return nullptr; }

} // namespace WebCore
