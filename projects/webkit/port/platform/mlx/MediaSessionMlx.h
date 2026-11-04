// The desktop's media controls for the pages' media: Voyager as an MPRIS
// player on MLXIPC (port/mlx/mediasession.mlx, org.mpris.MediaPlayer2.voyager).
// What plays comes from WebCore's media sessions (the pages' managers'
// current session and its NowPlayingInfo: the title, the duration and
// position, whether it seeks); the controls' commands (play, pause,
// next, seek) go back as the session's remote control commands, which
// the media element carries out; Raise goes to the embedder, which
// shows the tab. The bus is polled from the main thread ten times a
// second while a page is registered.
#pragma once

#if ENABLE(VIDEO)

#include <wtf/Function.h>
#include <wtf/MonotonicTime.h>
#include <wtf/RunLoop.h>
#include <wtf/Vector.h>
#include <wtf/WeakPtr.h>
#include <wtf/text/CString.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class Page;
class PlatformMediaSessionInterface;
struct NowPlayingInfo;

class MediaSessionMlx {
public:
    static MediaSessionMlx& singleton();

    // The embedder's pages (their media sessions are the players).
    void addPage(Page&);
    void removePage(Page&);
    // Raise: the embedder shows the page that plays.
    void setRaiseHandler(Function<void(Page&)>&&);
    // A player's state changed (play, pause, the end, a seek): what the
    // bus shows is refreshed on the next turn.
    void playbackChanged(bool seeked = false);

private:
    MediaSessionMlx();
    void tick();
    void refresh(bool seeked);
    void command(uint32_t kind, int64_t time);
    RefPtr<PlatformMediaSessionInterface> bestSession(Page*& page);
    void closeSession();
    CString coverFor(const NowPlayingInfo&);

    Vector<WeakPtr<Page>> m_pages;
    size_t m_session { 0 };
    MonotonicTime m_lastOpenAttempt;
    MonotonicTime m_stoppedSince;
    bool m_stopped { true };
    bool m_refreshPending { false };
    bool m_seekPending { false };
    unsigned m_ticks { 0 };
    String m_coverSource;
    CString m_coverUrl;
    RunLoop::Timer m_timer;
    Function<void(Page&)> m_raise;
};

} // namespace WebCore

#endif // ENABLE(VIDEO)
