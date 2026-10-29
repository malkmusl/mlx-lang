// The port's media controls on MLXIPC: see MediaSessionMlx.h.
#include "config.h"
#include "MediaSessionMlx.h"

#if ENABLE(VIDEO)

#include "Image.h"
#include "MediaSessionManagerInterface.h"
#include "NativeImage.h"
#include "NowPlayingInfo.h"
#include "Page.h"
#include "PlatformMediaSessionInterface.h"
#if ENABLE(MEDIA_SESSION)
#include "Document.h"
#include "LocalDOMWindow.h"
#include "LocalFrame.h"
#include "MediaSession.h"
#include "Navigator.h"
#include "NavigatorMediaSession.h"
#endif
#include "port/MlxWebPort.h"
#include <skia/core/SkColorSpace.h>
#include <skia/core/SkImage.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/text/CString.h>

namespace WebCore {

// The bus is polled this often, the position told every tenth time.
static constexpr Seconds pollInterval = 100_ms;
// Without a bus, opening is tried again after this long; a player that
// stopped leaves the bus after this long.
static constexpr Seconds retryInterval = 30_s;

MediaSessionMlx& MediaSessionMlx::singleton()
{
    static NeverDestroyed<MediaSessionMlx> session;
    return session.get();
}

MediaSessionMlx::MediaSessionMlx()
    : m_timer(RunLoop::mainSingleton(), "MediaSessionMlx::Timer"_s, [this] { tick(); })
{
}

void MediaSessionMlx::addPage(Page& page)
{
    m_pages.removeAllMatching([](auto& weak) { return !weak; });
    m_pages.append(page);
    if (!m_timer.isActive())
        m_timer.startRepeating(pollInterval);
}

void MediaSessionMlx::removePage(Page& page)
{
    m_pages.removeAllMatching([&page](auto& weak) { return !weak || weak.get() == &page; });
    if (m_pages.isEmpty()) {
        closeSession();
        m_timer.stop();
    }
}

void MediaSessionMlx::setRaiseHandler(Function<void(Page&)>&& raise)
{
    m_raise = WTF::move(raise);
}

void MediaSessionMlx::playbackChanged(bool seeked)
{
    if (seeked)
        m_seekPending = true;
    if (m_refreshPending)
        return;
    m_refreshPending = true;
    RunLoop::mainSingleton().dispatch([this] {
        m_refreshPending = false;
        bool seeked = m_seekPending;
        m_seekPending = false;
        refresh(seeked);
    });
}

void MediaSessionMlx::closeSession()
{
    if (!m_session)
        return;
    mlxweb_media_session_close(m_session);
    m_session = 0;
    m_coverSource = { };
    m_coverUrl = { };
}

// The session shown: one that plays, else the last that has something
// to tell.
RefPtr<PlatformMediaSessionInterface> MediaSessionMlx::bestSession(Page*& page)
{
    RefPtr<PlatformMediaSessionInterface> best;
    page = nullptr;
    for (auto& weak : m_pages) {
        RefPtr candidatePage = weak.get();
        if (!candidatePage)
            continue;
        RefPtr manager = candidatePage->mediaSessionManagerIfExists();
        if (!manager)
            continue;
        RefPtr session = manager->currentSession();
        if (!session || !session->nowPlayingInfo())
            continue;
        if (!best || session->isPlaying()) {
            best = session;
            page = candidatePage.get();
            if (session->isPlaying())
                break;
        }
    }
    return best;
}

static inline std::span<const char> text(const CString& string)
{
    return string.span();
}

// Whether the page's MediaSession (navigator.mediaSession) handles
// `action`: the controls' next and previous buttons.
static bool pageHandlesAction(Page* page, int action)
{
#if ENABLE(MEDIA_SESSION)
    if (!page)
        return false;
    RefPtr frame = dynamicDowncast<LocalFrame>(page->mainFrame());
    if (!frame)
        return false;
    RefPtr document = frame->document();
    if (!document)
        return false;
    RefPtr window = document->window();
    if (!window)
        return false;
    RefPtr session = NavigatorMediaSession::mediaSessionIfExists(window->navigator());
    return session && session->hasActionHandler(static_cast<MediaSessionAction>(action));
#else
    UNUSED_PARAM(page);
    UNUSED_PARAM(action);
    return false;
#endif
}

// The track's cover for the controls: the artwork WebCore loaded for
// the page's MediaSession (or the page's icon), its pixels written as a
// PNG file by the Mlx side once per artwork; the file:// URL is what the
// top bar shows. Empty without one.
CString MediaSessionMlx::coverFor(const NowPlayingInfo& info)
{
    if (!info.metadata.artwork || !info.metadata.artwork->image) {
        m_coverSource = { };
        m_coverUrl = { };
        return m_coverUrl;
    }
    if (info.metadata.artwork->src == m_coverSource)
        return m_coverUrl;
    m_coverSource = info.metadata.artwork->src;
    m_coverUrl = { };
    if (!m_session)
        return m_coverUrl;
    RefPtr nativeImage = info.metadata.artwork->image->nativeImage();
    if (!nativeImage)
        return m_coverUrl;
    auto& image = nativeImage->platformImage();
    if (!image)
        return m_coverUrl;
    int width = image->width();
    int height = image->height();
    if (width <= 0 || height <= 0 || width > 4096 || height > 4096)
        return m_coverUrl;
    size_t rowBytes = static_cast<size_t>(width) * 4;
    Vector<uint32_t> pixels(static_cast<size_t>(width) * height);
    auto pixelInfo = SkImageInfo::Make(width, height, kBGRA_8888_SkColorType, kPremul_SkAlphaType, SkColorSpace::MakeSRGB());
    if (!image->readPixels(pixelInfo, pixels.mutableSpan().data(), rowBytes, 0, 0))
        return m_coverUrl;
    char url[1024];
    size_t length = mlxweb_media_session_cover(m_session, pixels.span().data(), width, height, width, url, sizeof(url));
    if (length)
        m_coverUrl = CString(unsafeMakeSpan(url, length));
    return m_coverUrl;
}

void MediaSessionMlx::refresh(bool seeked)
{
    Page* page = nullptr;
    RefPtr session = bestSession(page);
    std::optional<NowPlayingInfo> info;
    if (session)
        info = session->nowPlayingInfo();
    auto now = MonotonicTime::now();
    if (!info) {
        // Nothing plays: the player leaves the bus after a while.
        if (!m_stopped) {
            m_stopped = true;
            m_stoppedSince = now;
            if (m_session)
                mlxweb_media_session_update(m_session, MLXWEB_MEDIA_STOPPED, nullptr, 0, nullptr, 0, nullptr, 0, nullptr, 0, nullptr, 0, 0, 0, 0, 0, 0, 0);
        } else if (m_session && now - m_stoppedSince > retryInterval)
            closeSession();
        return;
    }
    m_stopped = false;
    if (!m_session) {
        if (now - m_lastOpenAttempt < retryInterval)
            return;
        m_lastOpenAttempt = now;
        m_session = mlxweb_media_session_open();
        if (!m_session)
            return;
    }
    auto title = info->metadata.title.utf8();
    auto artist = info->metadata.artist.utf8();
    auto album = info->metadata.album.utf8();
    CString artwork = coverFor(*info);
    if (artwork.isNull() && info->metadata.artwork)
        artwork = info->metadata.artwork->src.utf8();
    int64_t length = std::isfinite(info->duration) && info->duration > 0 ? static_cast<int64_t>(info->duration * 1000000) : 0;
    int64_t position = std::isfinite(info->currentTime) ? static_cast<int64_t>(info->currentTime * 1000000) : 0;
    uint32_t status = info->isPlaying ? MLXWEB_MEDIA_PLAYING : MLXWEB_MEDIA_PAUSED;
#if ENABLE(MEDIA_SESSION)
    bool canNext = pageHandlesAction(page, static_cast<int>(MediaSessionAction::Nexttrack));
    bool canPrevious = pageHandlesAction(page, static_cast<int>(MediaSessionAction::Previoustrack));
#else
    bool canNext = pageHandlesAction(page, 0);
    bool canPrevious = canNext;
#endif
    mlxweb_media_session_update(m_session, status, text(title).data(), title.length(), text(artist).data(), artist.length(), text(album).data(), album.length(), text(artwork).data(), artwork.length(), nullptr, 0, length, position, info->supportsSeeking, canNext, canPrevious, seeked);
}

void MediaSessionMlx::command(uint32_t kind, int64_t time)
{
    Page* page = nullptr;
    RefPtr session = bestSession(page);
    if (kind == MLXWEB_MEDIA_COMMAND_RAISE) {
        if (page && m_raise)
            m_raise(*page);
        return;
    }
    if (!session)
        return;
    PlatformMediaSessionRemoteControlCommandType type = PlatformMediaSessionRemoteControlCommandType::NoCommand;
    PlatformMediaSessionRemoteCommandArgument argument;
    switch (kind) {
    case MLXWEB_MEDIA_COMMAND_PLAY:
        type = PlatformMediaSessionRemoteControlCommandType::PlayCommand;
        break;
    case MLXWEB_MEDIA_COMMAND_PAUSE:
        type = PlatformMediaSessionRemoteControlCommandType::PauseCommand;
        break;
    case MLXWEB_MEDIA_COMMAND_PLAY_PAUSE:
        type = PlatformMediaSessionRemoteControlCommandType::TogglePlayPauseCommand;
        break;
    case MLXWEB_MEDIA_COMMAND_STOP:
        type = PlatformMediaSessionRemoteControlCommandType::StopCommand;
        break;
    case MLXWEB_MEDIA_COMMAND_NEXT:
        type = PlatformMediaSessionRemoteControlCommandType::NextTrackCommand;
        break;
    case MLXWEB_MEDIA_COMMAND_PREVIOUS:
        type = PlatformMediaSessionRemoteControlCommandType::PreviousTrackCommand;
        break;
    case MLXWEB_MEDIA_COMMAND_SEEK_TO:
        type = PlatformMediaSessionRemoteControlCommandType::SeekToPlaybackPositionCommand;
        argument.time = static_cast<double>(time) / 1000000;
        break;
    case MLXWEB_MEDIA_COMMAND_SEEK_BY: {
        type = PlatformMediaSessionRemoteControlCommandType::SeekToPlaybackPositionCommand;
        double current = 0;
        if (auto info = session->nowPlayingInfo(); info && std::isfinite(info->currentTime))
            current = info->currentTime;
        argument.time = std::max(0.0, current + static_cast<double>(time) / 1000000);
        break;
    }
    default:
        return;
    }
    session->didReceiveRemoteControlCommand(type, argument);
    playbackChanged(type == PlatformMediaSessionRemoteControlCommandType::SeekToPlaybackPositionCommand);
}

void MediaSessionMlx::tick()
{
    if (!m_session)
        return;
    if (!mlxweb_media_session_connected(m_session)) {
        // The bus went: tried again later.
        closeSession();
        m_lastOpenAttempt = MonotonicTime::now();
        return;
    }
    uint32_t kind = 0;
    int64_t time = 0;
    while (mlxweb_media_session_poll(m_session, &kind, &time))
        command(kind, time);
    if (++m_ticks % 10 == 0 && !m_stopped)
        refresh(false);
}

} // namespace WebCore

#endif // ENABLE(VIDEO)
