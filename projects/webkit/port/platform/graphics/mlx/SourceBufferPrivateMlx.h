// The Mlx port's SourceBufferPrivate: the bytes a page appends to a
// SourceBuffer (MP4 initialization and media segments) read by the
// demuxer of port/mlx/media.mlx (std.mp4); the tracks go to WebCore as
// an initialization segment (MediaSampleMlx.h's tracks and descriptions)
// and to the player, every sample as a MediaSampleMlx to WebCore's
// track buffers, which hand them back (enqueueSample) to the player's
// queues in presentation order.
#pragma once

#if ENABLE(VIDEO) && ENABLE(MEDIA_SOURCE)

#include "ContentType.h"
#include "SourceBufferPrivate.h"
#include <wtf/HashMap.h>
#include <wtf/Logger.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class MediaPlayerPrivateMlx;
class MediaSourcePrivateMlx;

class SourceBufferPrivateMlx final : public SourceBufferPrivate, public CanMakeWeakPtr<SourceBufferPrivateMlx> {
public:
    static Ref<SourceBufferPrivateMlx> create(MediaSourcePrivateMlx&, const ContentType&);
    ~SourceBufferPrivateMlx();

    // The player's queue for the track has room again.
    void readyForMoreSamples(TrackID);

private:
    SourceBufferPrivateMlx(MediaSourcePrivateMlx&, const ContentType&);

    constexpr MediaPlatformType platformType() const final { return MediaPlatformType::GStreamer; }
    Ref<MediaPromise> appendInternal(Ref<SharedBuffer>&&) final;
    void resetParserStateInternal() final;
    void removedFromMediaSource() final;
    void flush(TrackID) final;
    void enqueueSample(Ref<MediaSample>&&, TrackID) final;
    void allSamplesInTrackEnqueued(TrackID) final;
    bool isReadyForMoreSamples(TrackID) final;
    void notifyClientWhenReadyForMoreSamples(TrackID) final;
    bool canSwitchToType(const ContentType&) final;
    const Logger& sourceBufferLogger() const final { return m_logger; }
    uint64_t sourceBufferLogIdentifier() final { return m_logIdentifier; }

    MediaPlayerPrivateMlx* player() const;
    bool processInitialization();
    bool processSamples();

    // A track of this buffer's stream: the demuxer's index and the ID
    // the player and WebCore know it by.
    struct Track {
        size_t index { 0 };
        TrackID id { 0 };
        uint32_t timescale { 1 };
        uint64_t editMediaTime { 0 };
        bool isVideo { false };
        FloatSize size;
    };

    ContentType m_contentType;
    size_t m_demuxer { 0 };
    Vector<uint8_t> m_pending;
    bool m_haveTracks { false };
    Vector<Track> m_tracks;
    const Ref<const Logger> m_logger;
    uint64_t m_logIdentifier { 0 };
};

} // namespace WebCore

#endif
