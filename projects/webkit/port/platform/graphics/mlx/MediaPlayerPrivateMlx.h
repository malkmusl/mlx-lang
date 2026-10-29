// The Mlx port's media engine (PORT.md, "Media"): MP4 files and Media
// Source Extensions segments read by std.mp4, AAC-LC decoded by std.aac
// and played on MLX Audio through std.audio, H.264 pictures ordered by
// std.h264 and decoded by the GPU's video decoder through std.gpu.video
// (Vulkan video; without one the video stays black), all through
// libmlxwebport.so (port/mlx/media.mlx).
//
// A file is loaded whole through the player's resource loader, then its
// tracks' samples come from the demuxer in order; a media source's
// samples come from its SourceBufferPrivateMlx through enqueueSample().
// Sound runs on a thread of its own that decodes the next audio sample
// and hands it to MLX Audio, which paces it; the frames handed over are
// the clock the video follows and currentTime() reports. Without an
// audio track the clock is the monotonic time since play().
#pragma once

#if ENABLE(VIDEO)

#include "MediaPlayerPrivate.h"
#include "PlatformTimeRanges.h"
#include <wtf/Condition.h>
#include <wtf/Deque.h>
#include <wtf/HashMap.h>
#include <wtf/Logger.h>
#include <wtf/Lock.h>
#include <wtf/RunLoop.h>
#include <wtf/Threading.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class CoordinatedPlatformLayerBufferProxy;
class MediaSampleMlx;
class NativeImage;
class MediaSourcePrivateMlx;
class MlxMediaResourceClient;
class PlatformMediaResource;
class PlatformMediaResourceLoader;
class SourceBufferPrivateMlx;

class MediaPlayerPrivateMlx final : public MediaPlayerPrivateInterface, public CanMakeWeakPtr<MediaPlayerPrivateMlx> {
    WTF_MAKE_TZONE_ALLOCATED(MediaPlayerPrivateMlx);
public:
    static void registerMediaEngine(MediaEngineRegistrar);
    static bool supportsContainer(const String& containerType);
    static bool supportsCodec(const String& codec);

    explicit MediaPlayerPrivateMlx(MediaPlayer*);
    ~MediaPlayerPrivateMlx();

    // What the loader of a file tells.
    void didReceiveData(std::span<const uint8_t>);
    void didFinishLoading();
    void didFailLoading();

#if ENABLE(MEDIA_SOURCE)
    // What a media source's buffers tell.
    void enqueueSample(Ref<MediaSampleMlx>&&, TrackID);
    void flushTrack(TrackID);
    bool isReadyForMoreSamples(TrackID);
    void setTrackEnded(TrackID);
    void mediaSourceDurationChanged();
    void mediaSourceBufferedChanged();
    void notifyWhenReadyForMoreSamples(TrackID, SourceBufferPrivateMlx&);
    // A track of a source buffer's initialization segment: the player
    // plays the first video and the first audio track it is given.
    TrackID allocateTrackID() { return ++m_lastTrackID; }
    void addSourceTrack(TrackID, bool isVideo, uint32_t timescale, uint64_t editMediaTime, std::span<const uint8_t> config, const Vector<Vector<uint8_t>>& parameterSets, const FloatSize&);
    const Logger& logger() const;
    uint64_t nextLogIdentifier() { return ++m_lastLogIdentifier; }
#endif

private:
    // MediaPlayerPrivateInterface.
    constexpr MediaPlayerType mediaPlayerType() const final { return MediaPlayerType::GStreamer; }
    void load(const String&) final;
    void load(const URL&, const LoadOptions&) final;
#if ENABLE(MEDIA_SOURCE)
    void load(const URL&, const LoadOptions&, MediaSourcePrivateClient&) final;
    void readyStateFromMediaSourceChanged() final;
    void characteristicsFromMediaSourceChanged() final;
    void mediaSourceHasRetrievedAllData() final;
    void notifyActiveSourceBuffersChanged() final;
#endif
#if ENABLE(MEDIA_STREAM)
    void load(MediaStreamPrivate&) final { }
#endif
    void cancelLoad() final;
    void prepareToPlay() final { }
    PlatformLayer* platformLayer() const final;
    void play() final;
    void pause() final;
    FloatSize naturalSize() const final { return m_naturalSize; }
    bool hasVideo() const final { return m_videoTrack.has_value(); }
    bool hasAudio() const final { return m_audioTrack.has_value(); }
    void setPageIsVisible(bool) final { }
    MediaTime duration() const final;
    MediaTime currentTime() const final;
    Ref<MediaTimePromise> seekToTarget(const SeekTarget&) final;
    bool paused() const final { return !m_playing; }
    void setRate(float) final;
    double rate() const final { return m_rate; }
    void setVolume(float) final;
    void setMuted(bool) final;
    MediaPlayer::NetworkState networkState() const final { return m_networkState; }
    MediaPlayer::ReadyState readyState() const final { return m_readyState; }
    MediaTime maxTimeSeekable() const final;
    const PlatformTimeRanges& buffered() const final { return m_buffered; }
    unsigned long long totalBytes() const final { return m_fileBytes.size(); }
    bool didLoadingProgress() const final;
    void paint(GraphicsContext&, const FloatRect&) final;
    DestinationColorSpace colorSpace() final;
    bool supportsAcceleratedRendering() const final { return true; }
    MediaPlayer::MovieLoadType movieLoadType() const final { return m_mediaSource ? MediaPlayer::MovieLoadType::StoredStream : MediaPlayer::MovieLoadType::Download; }
    String engineDescription() const final { return "Mlx"_s; }
    bool ended() const final { return m_ended; }
    unsigned decodedFrameCount() const final { return m_decodedFrames; }
    unsigned droppedFrameCount() const final { return m_droppedFrames; }
    bool supportsProgressMonitoring() const final { return !m_mediaSource; }

    // A track of the movie: the demuxer's index and what decodes it.
    struct Track {
        size_t index { 0 };
        TrackID id { 0 };
        uint32_t timescale { 1 };
        uint32_t codec { 0 };
        uint64_t editMediaTime { 0 };
    };

    void fileLoaded();
    void setNetworkState(MediaPlayer::NetworkState);
    void setReadyState(MediaPlayer::ReadyState);
    void updateStates();
    MediaTime clockTime() const;
    void startClock(const MediaTime& at);
    void stopClock();
    void seekClockTo(const MediaTime&);
    void startAudioThread();
    void stopAudioThread();
    void audioLoop();
    bool nextAudioSample(Vector<uint8_t>& bytes, MediaTime& presentationTime);
    bool nextVideoSample(Vector<uint8_t>& bytes, MediaTime& presentationTime, bool& sync);
    void videoTick();
    void decodeVideoSample(const Vector<uint8_t>&, const MediaTime&, bool sync);
    void presentFrame(uint32_t slot);
    void checkEnded();
    void didReachEnd();
    void resetDecoders();
    void flushQueues();
    void configureFromDemuxer();
    void wakeAudio();

    MediaPlayer* m_player;
    const Ref<const Logger> m_logger;
    MediaPlayer::NetworkState m_networkState { MediaPlayer::NetworkState::Empty };
    MediaPlayer::ReadyState m_readyState { MediaPlayer::ReadyState::HaveNothing };
    PlatformTimeRanges m_buffered;
    FloatSize m_naturalSize;
    MediaTime m_duration { MediaTime::invalidTime() };
    bool m_playing { false };
    bool m_ended { false };
    double m_rate { 1 };
    float m_volume { 1 };
    bool m_muted { false };
    mutable bool m_loadingProgressed { false };

    // The file (a plain load): its bytes as they come, parsed when all are
    // there.
    RefPtr<PlatformMediaResourceLoader> m_loader;
    RefPtr<PlatformMediaResource> m_resource;
    RefPtr<MlxMediaResourceClient> m_resourceClient;
    Vector<uint8_t> m_fileBytes;
    bool m_fileParsed { false };

    // The demuxer (port/mlx/media.mlx's) of a file, and the tracks played.
    size_t m_demuxer { 0 };
    std::optional<Track> m_audioTrack;
    std::optional<Track> m_videoTrack;
    mutable Lock m_demuxerLock;

#if ENABLE(MEDIA_SOURCE)
    RefPtr<MediaSourcePrivateMlx> m_mediaSource;
    // The samples enqueued per track, taken by the audio thread and the
    // video timer in presentation order.
    struct Queue {
        Deque<Ref<MediaSampleMlx>> samples;
        bool ended { false };
        WeakPtr<SourceBufferPrivateMlx> waiting;
    };
    HashMap<TrackID, Queue> m_queues WTF_GUARDED_BY_LOCK(m_queueLock);
    Lock m_queueLock;
#else
    Lock m_queueLock;
#endif

    // Sound: the decoder and MLX Audio output of the audio thread.
    size_t m_aacDecoder { 0 };
    size_t m_audioOutput { 0 };
    uint32_t m_audioRate { 0 };
    uint32_t m_audioChannels { 0 };
    RefPtr<Thread> m_audioThread;
    Lock m_audioLock;
    Condition m_audioCondition;
    bool m_audioThreadShouldExit { false };
    bool m_audioStarved { false };
    // The clock: the time at the last start or seek, and the frames
    // written since (audio) or the monotonic time it started (no audio).
    MediaTime m_clockBase { MediaTime::zeroTime() };
    std::atomic<uint64_t> m_framesAtClockBase { 0 };
    std::atomic<uint64_t> m_framesWritten { 0 };
    MonotonicTime m_clockStarted;
    bool m_clockRunning { false };

    // Video: the H.264 picture order (port/mlx/media.mlx's) and the layer
    // the frames go to; the timer that takes the samples due.
    size_t m_h264 { 0 };
    bool m_h264Configured { false };
    // The GPU's decoder (port/mlx/media.mlx's), opened at the first
    // picture; 0 when no device decodes.
    size_t m_videoDecoder { 0 };
    bool m_videoDecoderTried { false };
    RefPtr<NativeImage> m_lastFrame;
    RefPtr<CoordinatedPlatformLayerBufferProxy> m_layerProxy;
    RunLoop::Timer m_videoTimer;
    MediaTime m_lastVideoTime { MediaTime::invalidTime() };
    unsigned m_decodedFrames { 0 };
    unsigned m_droppedFrames { 0 };
    bool m_videoEnded { false };
    bool m_audioEnded { false };
    TrackID m_lastTrackID { 0 };
    uint64_t m_lastLogIdentifier { 0 };
};

} // namespace WebCore

#endif // ENABLE(VIDEO)
