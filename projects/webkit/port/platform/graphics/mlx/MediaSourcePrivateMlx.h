// The Mlx port's MediaSourcePrivate: the media source of a
// MediaPlayerPrivateMlx playing through Media Source Extensions; its
// source buffers are SourceBufferPrivateMlx. Everything runs on the main
// thread (WebCore's MediaSource dispatcher).
#pragma once

#if ENABLE(VIDEO) && ENABLE(MEDIA_SOURCE)

#include "MediaSourcePrivate.h"
#include <wtf/WeakPtr.h>

namespace WebCore {

class MediaPlayerPrivateMlx;

class MediaSourcePrivateMlx final : public MediaSourcePrivate {
public:
    static Ref<MediaSourcePrivateMlx> create(MediaSourcePrivateClient&, MediaPlayerPrivateMlx&);
    ~MediaSourcePrivateMlx();

    MediaPlayerPrivateMlx* playerMlx() const { return m_player.get(); }

    constexpr MediaPlatformType platformType() const final { return MediaPlatformType::GStreamer; }
    RefPtr<MediaPlayerPrivateInterface> player() const final;
    void setPlayer(MediaPlayerPrivateInterface*) final;
    AddStatus addSourceBuffer(const ContentType&, const MediaSourceConfiguration&, RefPtr<SourceBufferPrivate>&) final;
    void notifyActiveSourceBuffersChanged() final;
    void durationChanged(const MediaTime&) final;
    void bufferedChanged(PlatformTimeRanges&&) final;
    void markEndOfStream(EndOfStreamStatus) final;

private:
    MediaSourcePrivateMlx(MediaSourcePrivateClient&, MediaPlayerPrivateMlx&);

    WeakPtr<MediaPlayerPrivateMlx> m_player;
};

} // namespace WebCore

#endif
