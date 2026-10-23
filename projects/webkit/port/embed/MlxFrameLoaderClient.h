// The frame loader client of the Mlx port's single-process embedding
// (PORT.md, "One process first"): what WebKit2's WebLocalFrameLoaderClient
// and WebKitLegacy's WebFrameLoaderClient were to a frame, without a UI
// process to ask. Navigations and responses are allowed (the embedder has
// no policy yet), the main resource's data goes to the document
// (DocumentLoader::commitData), and a new page's frame gets a view of the
// size the embedder gives. The rest is EmptyFrameLoaderClient's nothing,
// written out because that class's functions are final and it calls no
// policy function (a load through it never starts).
//
//     PageConfiguration::LocalMainFrameCreationParameters {
//         [](auto&, auto& frameLoader) { return makeUniqueRefWithoutRefCountedCheck<MlxFrameLoaderClient>(frameLoader, IntSize(800, 600)); }, ... }
//
// STUB (M5: frames in frames, plug-ins, history, downloads, forms' policy):
// createFrame makes no frame (an iframe stays empty), there is no back and
// forward list and no download; a new window is the chrome client's
// (createWindow: a tab of Voyager's).
#pragma once

#include "IntSize.h"
#include "LocalFrameLoaderClient.h"

namespace WebCore {

class MlxFrameLoaderClient : public LocalFrameLoaderClient {
public:
    WEBCORE_EXPORT MlxFrameLoaderClient(FrameLoader&, IntSize viewSize);
    WEBCORE_EXPORT ~MlxFrameLoaderClient();

    // Called once the frame's load has finished (or failed): the main
    // frame's dispatchDidFinishLoad or dispatchDidFailLoad.
    void setLoadFinishedCallback(Function<void(bool succeeded)>&& callback) { m_loadFinished = WTF::move(callback); }

    // The embedder's name, version and window system in every frame's
    // user agent ("Voyager", "0.1", "Wayland"): in WebKit's compatibility
    // string after the platform, as "Voyager/0.1 (WebKit 2.54.1; Wayland)"
    // (UserAgentMlx.cpp).
    WEBCORE_EXPORT static void setApplicationNameForUserAgent(const String& name, const String& version, const String& windowSystem = String());

private:
    Ref<DocumentLoader> createDocumentLoader(ResourceRequest&&, SubstituteData&&, ResourceRequest&&) override;
    Ref<DocumentLoader> createDocumentLoader(ResourceRequest&&, SubstituteData&&) override;

    bool hasWebView() const override;
    void makeRepresentation(DocumentLoader*) override;
    void forceLayoutForNonHTML() override;
    void setCopiesOnScroll() override;
    void detachedFromParent2() override;
    void detachedFromParent3() override;
    void convertMainResourceLoadToDownload(DocumentLoader*, const ResourceRequest&, const ResourceResponse&) override;

    void assignIdentifierToInitialRequest(ResourceLoaderIdentifier, DocumentLoader*, const ResourceRequest&) override;
    bool shouldUseCredentialStorage(DocumentLoader*, ResourceLoaderIdentifier) override;
    void dispatchWillSendRequest(DocumentLoader*, ResourceLoaderIdentifier, ResourceRequest&, const ResourceResponse&) override;
    void dispatchDidReceiveAuthenticationChallenge(DocumentLoader*, ResourceLoaderIdentifier, const AuthenticationChallenge&) override;
#if USE(PROTECTION_SPACE_AUTH_CALLBACK)
    bool canAuthenticateAgainstProtectionSpace(DocumentLoader*, ResourceLoaderIdentifier, const ProtectionSpace&) override;
#endif
    void dispatchDidReceiveResponse(DocumentLoader*, ResourceLoaderIdentifier, const ResourceResponse&) override;
    void dispatchDidReceiveContentLength(DocumentLoader*, ResourceLoaderIdentifier, int) override;
    void dispatchDidFinishLoading(DocumentLoader*, ResourceLoaderIdentifier) override;
    void dispatchDidFailLoading(DocumentLoader*, ResourceLoaderIdentifier, const ResourceError&) override;
    bool dispatchDidLoadResourceFromMemoryCache(DocumentLoader*, const ResourceRequest&, const ResourceResponse&, int) override;

    void dispatchDidDispatchOnloadEvents() override;
    void dispatchDidReceiveServerRedirectForProvisionalLoad() override;
    void dispatchDidCancelClientRedirect() override;
    void dispatchWillPerformClientRedirect(const URL&, double, WallTime, LockBackForwardList) override;
    void dispatchDidChangeLocationWithinPage() override;
    void dispatchDidPushStateWithinPage() override;
    void dispatchDidReplaceStateWithinPage() override;
    void dispatchDidPopStateWithinPage() override;
    void dispatchWillClose() override;
    void dispatchDidStartProvisionalLoad() override;
    void dispatchDidReceiveTitle(const StringWithDirection&) override;
    void dispatchDidCommitLoad(std::optional<HasInsecureContent>, std::optional<UsedLegacyTLS>, std::optional<WasPrivateRelayed>) override;
    void dispatchDidFailProvisionalLoad(const ResourceError&, WillContinueLoading, WillInternallyHandleFailure) override;
    void dispatchDidFailLoad(const ResourceError&) override;
    void dispatchDidFinishDocumentLoad() override;
    void dispatchDidFinishLoad() override;
    void dispatchDidReachLayoutMilestone(OptionSet<LayoutMilestone>) override;
    void dispatchDidReachVisuallyNonEmptyState() override;

    LocalFrame* dispatchCreatePage(const NavigationAction&, NewFrameOpenerPolicy, const String&) override;
    void dispatchShow() override;

    void dispatchDecidePolicyForResponse(const ResourceResponse&, const ResourceRequest&, const String&, FramePolicyFunction&&) override;
    void dispatchDecidePolicyForNewWindowAction(const NavigationAction&, const ResourceRequest&, FormState*, const String&, std::optional<HitTestResult>&&, FramePolicyFunction&&) override;
    void dispatchDecidePolicyForNavigationAction(const NavigationAction&, const ResourceRequest&, const ResourceResponse& redirectResponse, FormState*, const String&, std::optional<NavigationIdentifier>, std::optional<HitTestResult>&&, bool, NavigationUpgradeToHTTPSBehavior, SandboxFlags, PolicyDecisionMode, FramePolicyFunction&&) override;
    void updateSandboxFlags(SandboxFlags) override;
    void updateOpener(std::optional<FrameIdentifier>) override;
    void setPrinting(bool, FloatSize, FloatSize, float, AdjustViewSize) override;
    void cancelPolicyCheck() override;
    void dispatchUnableToImplementPolicy(const ResourceError&) override;

    void dispatchWillSendSubmitEvent(Ref<FormState>&&) override;
    void dispatchWillSubmitForm(FormState&, URL&& requestURL, String&& method, CompletionHandler<void()>&&) override;

    void revertToProvisionalState(DocumentLoader*) override;
    void setMainDocumentError(DocumentLoader*, const ResourceError&) override;
    void setMainFrameDocumentReady(bool) override;
    void startDownload(const ResourceRequest&, const String&, FromDownloadAttribute = FromDownloadAttribute::No) override;
    void willChangeTitle(DocumentLoader*) override;
    void didChangeTitle(DocumentLoader*) override;
    void willReplaceMultipartContent() override;
    void didReplaceMultipartContent() override;

    void committedLoad(DocumentLoader*, const SharedBuffer&) override;
    void finishedLoading(DocumentLoader*) override;
    void loadStorageAccessQuirksIfNeeded() override;
    bool shouldFallBack(const ResourceError&) const override;

    bool canHandleRequest(const ResourceRequest&) const override;
    bool canShowMIMEType(const String&) const override;
    bool canShowMIMETypeAsHTML(const String&) const override;
    bool representationExistsForURLScheme(StringView) const override;
    String generatedMIMETypeForURLScheme(StringView) const override;

    void frameLoadCompleted() override;
    void restoreViewState() override;
    void provisionalLoadStarted() override;
    void didFinishLoad() override;
    void prepareForDataSourceReplacement() override;
    void updateCachedDocumentLoader(DocumentLoader&) override;
    void setTitle(const StringWithDirection&, const URL&) override;

    String userAgent(const URL&) const override;

    void savePlatformDataToCachedFrame(CachedFrame*) override;
    void transitionToCommittedFromCachedFrame(CachedFrame*) override;
    void transitionToCommittedForNewPage(InitializingIframe) override;
    void didRestoreFromBackForwardCache() override;

    void updateGlobalHistory() override;
    void updateGlobalHistoryRedirectLinks() override;
    ShouldGoToHistoryItem shouldGoToHistoryItem(HistoryItem&, IsSameDocumentNavigation) const override;
    bool supportsAsyncShouldGoToHistoryItem() const override;
    void shouldGoToHistoryItemAsync(HistoryItem&, CompletionHandler<void(ShouldGoToHistoryItem)>&&) const override;
    void dispatchGoToBackForwardItemAtIndex(int) override;
    void dispatchEnqueueHistoryTraversalDelta(int) override;
    void saveViewStateToItem(HistoryItem&) override;
    bool canCachePage() const override;

    RefPtr<LocalFrame> createFrame(const AtomString&, HTMLFrameOwnerElement&) override;
    RefPtr<Widget> createPlugin(HTMLPlugInElement&, const URL&, const Vector<AtomString>&, const Vector<AtomString>&, const String&, bool) override;
    void redirectDataToPlugin(Widget&) override;
    ObjectContentType objectContentType(const URL&, const String&) override;
    AtomString overrideMediaType() const override;
    void dispatchDidClearWindowObjectInWorld(DOMWrapperWorld&) override;

    Ref<FrameNetworkingContext> createNetworkingContext() override;
    void prefetchDNS(const String&) override;
    void sendH2Ping(const URL&, CompletionHandler<void(Expected<Seconds, ResourceError>&&)>&&) override;
    void dispatchLoadEventToOwnerElementInAnotherProcess() override;
    RefPtr<HistoryItem> createHistoryItemTree(bool, BackForwardItemIdentifier) const override;

    void loadFinished(bool succeeded);

    IntSize m_viewSize;
    Function<void(bool)> m_loadFinished;
};

} // namespace WebCore
