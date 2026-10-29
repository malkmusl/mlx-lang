// The Mlx port's media engine: see MediaPlayerPrivateMlx.h.
#include "config.h"
#include "MediaPlayerPrivateMlx.h"

#if ENABLE(VIDEO)

#include "ContentType.h"
#include "DestinationColorSpace.h"
#include "GraphicsContext.h"
#include "MediaPlayer.h"
#include "MediaSampleMlx.h"
#include "NetworkLoadMetrics.h"
#include "PlatformMediaResourceLoader.h"
#include "ResourceError.h"
#include "ResourceRequest.h"
#include "ResourceResponse.h"
#include "SharedBuffer.h"
#include "port/MlxWebPort.h"
#include <wtf/MainThread.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/StringToIntegerConversion.h>

#if ENABLE(MEDIA_SOURCE)
#include "MediaSourcePrivateClient.h"
#include "MediaSourcePrivateMlx.h"
#include "SourceBufferPrivateMlx.h"
#endif

#if USE(COORDINATED_GRAPHICS)
#include "CoordinatedPlatformLayerBufferProxy.h"
#endif

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(MediaPlayerPrivateMlx);

// The latency asked of MLX Audio, and what the clock takes off the frames
// handed to it.
static constexpr uint32_t audioLatencyMilliseconds = 100;
static constexpr size_t audioQueueLimit = 64;
static constexpr size_t videoQueueLimit = 32;

// ---- The factory ----

class MediaPlayerFactoryMlx final : public MediaPlayerFactory {
private:
    MediaPlayerEnums::MediaEngineIdentifier identifier() const final { return MediaPlayerEnums::MediaEngineIdentifier::GStreamer; }

    Ref<MediaPlayerPrivateInterface> createMediaEnginePlayer(MediaPlayer& player) const final
    {
        return adoptRef(*new MediaPlayerPrivateMlx(&player));
    }

    void getSupportedTypes(HashSet<String>& types) const final
    {
        types.add("video/mp4"_s);
        types.add("audio/mp4"_s);
        types.add("audio/aac"_s);
        types.add("audio/x-m4a"_s);
        types.add("video/x-m4v"_s);
    }

    MediaPlayer::SupportsType supportsTypeAndCodecs(const MediaEngineSupportParameters& parameters) const final
    {
        if (parameters.platformType == PlatformMediaDecodingType::MediaStream)
            return MediaPlayer::SupportsType::IsNotSupported;
        auto container = parameters.type.containerType();
        if (container.isEmpty() || !MediaPlayerPrivateMlx::supportsContainer(container))
            return MediaPlayer::SupportsType::IsNotSupported;
        auto codecs = parameters.type.codecs();
        if (codecs.isEmpty())
            return MediaPlayer::SupportsType::MayBeSupported;
        for (auto& codec : codecs) {
            if (!MediaPlayerPrivateMlx::supportsCodec(codec))
                return MediaPlayer::SupportsType::IsNotSupported;
        }
        return MediaPlayer::SupportsType::IsSupported;
    }
};

void MediaPlayerPrivateMlx::registerMediaEngine(MediaEngineRegistrar registrar)
{
    registrar(makeUnique<MediaPlayerFactoryMlx>());
}

bool MediaPlayerPrivateMlx::supportsContainer(const String& containerType)
{
    auto type = containerType.convertToASCIILowercase();
    return type == "video/mp4"_s || type == "audio/mp4"_s || type == "audio/aac"_s || type == "audio/x-m4a"_s || type == "video/x-m4v"_s;
}

bool MediaPlayerPrivateMlx::supportsCodec(const String& codec)
{
    auto lower = codec.convertToASCIILowercase().trim(isASCIIWhitespace<char16_t>);
    // AVC in any profile, as the decoder reads every frame-coded stream.
    if (lower.startsWith("avc1"_s) || lower.startsWith("avc3"_s))
        return true;
    // AAC-LC, and HE-AAC played as its LC core.
    if (lower == "mp4a.40.2"_s || lower == "mp4a.40.02"_s || lower == "mp4a.40.5"_s || lower == "mp4a.40.05"_s || lower == "mp4a.40.29"_s || lower == "mp4a.40"_s || lower == "mp4a"_s)
        return true;
    return false;
}

// ---- The loader's client ----

class MlxMediaResourceClient final : public PlatformMediaResourceClient {
public:
    static Ref<MlxMediaResourceClient> create(MediaPlayerPrivateMlx& player) { return adoptRef(*new MlxMediaResourceClient(player)); }

private:
    explicit MlxMediaResourceClient(MediaPlayerPrivateMlx& player)
        : m_player(player)
    {
    }

    void dataReceived(PlatformMediaResource&, const SharedBuffer& buffer) final
    {
        if (RefPtr player = m_player.get())
            player->didReceiveData(buffer.span());
    }
    void loadFailed(PlatformMediaResource&, const ResourceError&) final
    {
        if (RefPtr player = m_player.get())
            player->didFailLoading();
    }
    void accessControlCheckFailed(PlatformMediaResource&, const ResourceError&) final
    {
        if (RefPtr player = m_player.get())
            player->didFailLoading();
    }
    void loadFinished(PlatformMediaResource&, const NetworkLoadMetrics&) final
    {
        if (RefPtr player = m_player.get())
            player->didFinishLoading();
    }

    WeakPtr<MediaPlayerPrivateMlx> m_player;
};

// ---- The player ----

MediaPlayerPrivateMlx::MediaPlayerPrivateMlx(MediaPlayer* player)
    : m_player(player)
    , m_logger(Logger::create(this))
    , m_videoTimer(RunLoop::mainSingleton(), "MediaPlayerPrivateMlx::VideoTimer"_s, this, &MediaPlayerPrivateMlx::videoTick)
{
#if USE(COORDINATED_GRAPHICS)
    m_layerProxy = CoordinatedPlatformLayerBufferProxy::create();
#endif
}

MediaPlayerPrivateMlx::~MediaPlayerPrivateMlx()
{
    cancelLoad();
}

void MediaPlayerPrivateMlx::load(const String& urlString)
{
    load(URL { urlString }, { });
}

void MediaPlayerPrivateMlx::load(const URL& url, const LoadOptions&)
{
    cancelLoad();
    m_networkState = MediaPlayer::NetworkState::Loading;
    m_readyState = MediaPlayer::ReadyState::HaveNothing;
    m_player->networkStateChanged();
    m_player->readyStateChanged();
    m_loader = m_player->mediaResourceLoader();
    m_resourceClient = MlxMediaResourceClient::create(*this);
    ResourceRequest request { URL { url } };
    m_resource = m_loader->requestResource(WTF::move(request), PlatformMediaResourceLoader::BufferData);
    if (!m_resource) {
        setNetworkState(MediaPlayer::NetworkState::NetworkError);
        return;
    }
    m_resource->setClient(m_resourceClient.copyRef());
}

#if ENABLE(MEDIA_SOURCE)
void MediaPlayerPrivateMlx::load(const URL&, const LoadOptions&, MediaSourcePrivateClient& client)
{
    cancelLoad();
    m_networkState = MediaPlayer::NetworkState::Loading;
    m_readyState = MediaPlayer::ReadyState::HaveNothing;
    m_player->networkStateChanged();
    m_player->readyStateChanged();
    m_mediaSource = MediaSourcePrivateMlx::create(client, *this);
    client.setPrivateAndOpen(*m_mediaSource);
}

void MediaPlayerPrivateMlx::readyStateFromMediaSourceChanged()
{
    if (!m_mediaSource)
        return;
    auto state = m_mediaSource->mediaPlayerReadyState();
    if (state >= MediaPlayer::ReadyState::HaveMetadata && m_networkState == MediaPlayer::NetworkState::Loading)
        setNetworkState(MediaPlayer::NetworkState::Loaded);
    setReadyState(state);
}

void MediaPlayerPrivateMlx::characteristicsFromMediaSourceChanged()
{
    m_player->characteristicChanged();
}

void MediaPlayerPrivateMlx::mediaSourceHasRetrievedAllData()
{
    Locker locker { m_queueLock };
    for (auto& queue : m_queues.values())
        queue.ended = true;
    wakeAudio();
}

void MediaPlayerPrivateMlx::notifyActiveSourceBuffersChanged()
{
    m_player->activeSourceBuffersChanged();
}

void MediaPlayerPrivateMlx::enqueueSample(Ref<MediaSampleMlx>&& sample, TrackID trackID)
{
    {
        Locker locker { m_queueLock };
        auto& queue = m_queues.ensure(trackID, [] { return Queue { }; }).iterator->value;
        queue.samples.append(WTF::move(sample));
        queue.ended = false;
    }
    wakeAudio();
}

void MediaPlayerPrivateMlx::flushTrack(TrackID trackID)
{
    Locker locker { m_queueLock };
    auto it = m_queues.find(trackID);
    if (it == m_queues.end())
        return;
    it->value.samples.clear();
    it->value.ended = false;
}

bool MediaPlayerPrivateMlx::isReadyForMoreSamples(TrackID trackID)
{
    Locker locker { m_queueLock };
    auto it = m_queues.find(trackID);
    if (it == m_queues.end())
        return true;
    bool isVideo = m_videoTrack && m_videoTrack->id == trackID;
    return it->value.samples.size() < (isVideo ? videoQueueLimit : audioQueueLimit);
}

void MediaPlayerPrivateMlx::setTrackEnded(TrackID trackID)
{
    Locker locker { m_queueLock };
    auto& queue = m_queues.ensure(trackID, [] { return Queue { }; }).iterator->value;
    queue.ended = true;
    wakeAudio();
}

void MediaPlayerPrivateMlx::notifyWhenReadyForMoreSamples(TrackID trackID, SourceBufferPrivateMlx& buffer)
{
    Locker locker { m_queueLock };
    auto& queue = m_queues.ensure(trackID, [] { return Queue { }; }).iterator->value;
    queue.waiting = buffer;
}

void MediaPlayerPrivateMlx::mediaSourceDurationChanged()
{
    m_player->durationChanged();
}

const Logger& MediaPlayerPrivateMlx::logger() const
{
    return m_logger;
}

void MediaPlayerPrivateMlx::addSourceTrack(TrackID id, bool isVideo, uint32_t timescale, uint64_t editMediaTime, std::span<const uint8_t> config, const Vector<Vector<uint8_t>>& parameterSets, const FloatSize& size)
{
    Track track;
    track.index = 0;
    track.id = id;
    track.timescale = timescale ? timescale : 1;
    track.editMediaTime = editMediaTime;
    if (isVideo) {
        if (m_videoTrack)
            return;
        track.codec = MLXWEB_MEDIA_H264;
        m_videoTrack = track;
        m_naturalSize = size;
        if (!m_h264)
            m_h264 = mlxweb_media_h264_create();
        if (m_h264) {
            for (auto& set : parameterSets)
                mlxweb_media_h264_parameter_set(m_h264, set.span().data(), set.size());
            m_h264Configured = !parameterSets.isEmpty();
        }
        m_player->sizeChanged();
    } else {
        if (m_audioTrack)
            return;
        track.codec = MLXWEB_MEDIA_AAC;
        if (m_aacDecoder)
            mlxweb_media_aac_destroy(m_aacDecoder);
        m_aacDecoder = mlxweb_media_aac_create(config.data(), config.size());
        if (!m_aacDecoder)
            return;
        m_audioTrack = track;
    }
    m_player->characteristicChanged();
    if (m_playing) {
        if (m_audioTrack)
            startAudioThread();
        if (m_videoTrack && !m_videoTimer.isActive())
            m_videoTimer.startRepeating(16_ms);
    }
}

void MediaPlayerPrivateMlx::mediaSourceBufferedChanged()
{
    if (!m_mediaSource)
        return;
    m_buffered = m_mediaSource->buffered();
    m_loadingProgressed = true;
}
#endif

void MediaPlayerPrivateMlx::cancelLoad()
{
    stopAudioThread();
    m_videoTimer.stop();
    m_playing = false;
    m_clockRunning = false;
    if (m_resource) {
        m_resource->setClient(nullptr);
        m_resource->shutdown();
        m_resource = nullptr;
    }
    m_resourceClient = nullptr;
    m_loader = nullptr;
    m_fileBytes = { };
    m_fileParsed = false;
#if ENABLE(MEDIA_SOURCE)
    if (m_mediaSource) {
        m_mediaSource->setPlayer(nullptr);
        m_mediaSource = nullptr;
    }
    {
        Locker locker { m_queueLock };
        m_queues.clear();
    }
#endif
    {
        Locker locker { m_demuxerLock };
        if (m_demuxer) {
            mlxweb_media_demuxer_destroy(m_demuxer);
            m_demuxer = 0;
        }
    }
    resetDecoders();
    if (m_aacDecoder) {
        mlxweb_media_aac_destroy(m_aacDecoder);
        m_aacDecoder = 0;
    }
    if (m_h264) {
        mlxweb_media_h264_destroy(m_h264);
        m_h264 = 0;
    }
    if (m_audioOutput) {
        mlxweb_audio_close(m_audioOutput);
        m_audioOutput = 0;
    }
    m_audioTrack = std::nullopt;
    m_videoTrack = std::nullopt;
    m_h264Configured = false;
    m_ended = false;
}

// ---- A file ----

void MediaPlayerPrivateMlx::didReceiveData(std::span<const uint8_t> bytes)
{
    m_fileBytes.append(bytes);
    m_loadingProgressed = true;
}

void MediaPlayerPrivateMlx::didFailLoading()
{
    setNetworkState(MediaPlayer::NetworkState::NetworkError);
}

void MediaPlayerPrivateMlx::didFinishLoading()
{
    if (m_fileParsed)
        return;
    m_fileParsed = true;
    fileLoaded();
}

void MediaPlayerPrivateMlx::fileLoaded()
{
    Locker locker { m_demuxerLock };
    if (!m_demuxer)
        m_demuxer = mlxweb_media_demuxer_create();
    size_t used = 0;
    auto status = mlxweb_media_demuxer_parse(m_demuxer, m_fileBytes.span().data(), m_fileBytes.size(), &used);
    if (status != MLXWEB_MEDIA_OK || !mlxweb_media_track_count(m_demuxer)) {
        locker.unlockEarly();
        setNetworkState(MediaPlayer::NetworkState::FormatError);
        return;
    }
    locker.unlockEarly();
    configureFromDemuxer();
    if (!m_audioTrack && !m_videoTrack) {
        setNetworkState(MediaPlayer::NetworkState::FormatError);
        return;
    }
    m_buffered = PlatformTimeRanges(MediaTime::zeroTime(), m_duration.isValid() ? m_duration : MediaTime::zeroTime());
    m_loadingProgressed = true;
    setNetworkState(MediaPlayer::NetworkState::Loaded);
    m_player->durationChanged();
    m_player->sizeChanged();
    m_player->characteristicChanged();
    setReadyState(MediaPlayer::ReadyState::HaveEnoughData);
}

// The tracks played (the first H.264 video and the first AAC audio
// track), their decoders, the natural size and duration.
void MediaPlayerPrivateMlx::configureFromDemuxer()
{
    Locker locker { m_demuxerLock };
    size_t count = mlxweb_media_track_count(m_demuxer);
    MediaTime duration = MediaTime::zeroTime();
    for (size_t index = 0; index < count; ++index) {
        Track track;
        track.index = index;
        track.id = index + 1;
        track.timescale = mlxweb_media_track_timescale(m_demuxer, index);
        track.codec = mlxweb_media_track_codec(m_demuxer, index);
        track.editMediaTime = mlxweb_media_track_edit_media_time(m_demuxer, index);
        if (!track.timescale)
            continue;
        auto kind = mlxweb_media_track_kind(m_demuxer, index);
        MediaTime trackDuration(static_cast<int64_t>(mlxweb_media_track_duration(m_demuxer, index)), track.timescale);
        if (trackDuration > duration)
            duration = trackDuration;
        if (kind == MLXWEB_MEDIA_VIDEO && track.codec == MLXWEB_MEDIA_H264 && !m_videoTrack) {
            m_videoTrack = track;
            m_naturalSize = FloatSize(mlxweb_media_track_width(m_demuxer, index), mlxweb_media_track_height(m_demuxer, index));
        } else if (kind == MLXWEB_MEDIA_AUDIO && track.codec == MLXWEB_MEDIA_AAC && !m_audioTrack) {
            m_audioTrack = track;
            size_t length = 0;
            auto* config = mlxweb_media_track_config(m_demuxer, index, &length);
            if (m_aacDecoder)
                mlxweb_media_aac_destroy(m_aacDecoder);
            m_aacDecoder = mlxweb_media_aac_create(config, length);
            if (!m_aacDecoder)
                m_audioTrack = std::nullopt;
        }
    }
    if (duration > MediaTime::zeroTime())
        m_duration = duration;
    if (m_videoTrack) {
        if (!m_h264)
            m_h264 = mlxweb_media_h264_create();
        m_h264Configured = false;
        if (m_h264) {
            for (size_t set = 0;; ++set) {
                size_t length = 0;
                auto* bytes = mlxweb_media_track_parameter_set(m_demuxer, m_videoTrack->index, set, &length);
                if (!bytes)
                    break;
                mlxweb_media_h264_parameter_set(m_h264, bytes, length);
                m_h264Configured = true;
            }
        }
    }
}

void MediaPlayerPrivateMlx::setNetworkState(MediaPlayer::NetworkState state)
{
    if (m_networkState == state)
        return;
    m_networkState = state;
    m_player->networkStateChanged();
}

void MediaPlayerPrivateMlx::setReadyState(MediaPlayer::ReadyState state)
{
    if (m_readyState == state)
        return;
    m_readyState = state;
    m_player->readyStateChanged();
}

void MediaPlayerPrivateMlx::updateStates()
{
}

bool MediaPlayerPrivateMlx::didLoadingProgress() const
{
    bool progressed = m_loadingProgressed;
    m_loadingProgressed = false;
    return progressed;
}

MediaTime MediaPlayerPrivateMlx::duration() const
{
#if ENABLE(MEDIA_SOURCE)
    if (m_mediaSource)
        return m_mediaSource->duration();
#endif
    return m_duration.isValid() ? m_duration : MediaTime::zeroTime();
}

MediaTime MediaPlayerPrivateMlx::maxTimeSeekable() const
{
    auto total = duration();
    return total.isValid() && !total.isPositiveInfinite() ? total : MediaTime::zeroTime();
}

// ---- The clock ----

MediaTime MediaPlayerPrivateMlx::clockTime() const
{
    if (!m_clockRunning)
        return m_clockBase;
    if (m_audioOutput && m_audioRate && !m_audioStarved) {
        uint64_t frames = m_framesWritten.load() - m_framesAtClockBase.load();
        double seconds = static_cast<double>(frames) / m_audioRate - audioLatencyMilliseconds / 1000.0;
        if (seconds < 0)
            seconds = 0;
        return m_clockBase + MediaTime::createWithDouble(seconds, 1000000);
    }
    double elapsed = (MonotonicTime::now() - m_clockStarted).seconds() * m_rate;
    return m_clockBase + MediaTime::createWithDouble(elapsed, 1000000);
}

MediaTime MediaPlayerPrivateMlx::currentTime() const
{
    auto time = clockTime();
    auto total = duration();
    if (total.isValid() && !total.isPositiveInfinite() && time > total)
        return total;
    return time;
}

void MediaPlayerPrivateMlx::startClock(const MediaTime& at)
{
    m_clockBase = at;
    m_framesAtClockBase = m_framesWritten.load();
    m_clockStarted = MonotonicTime::now();
    m_clockRunning = true;
}

void MediaPlayerPrivateMlx::stopClock()
{
    if (!m_clockRunning)
        return;
    m_clockBase = clockTime();
    m_clockRunning = false;
}

void MediaPlayerPrivateMlx::seekClockTo(const MediaTime& time)
{
    bool running = m_clockRunning;
    m_clockBase = time;
    m_framesAtClockBase = m_framesWritten.load();
    m_clockStarted = MonotonicTime::now();
    m_clockRunning = running;
}

// ---- Playing ----

void MediaPlayerPrivateMlx::play()
{
    if (m_playing)
        return;
    if (m_ended) {
        m_ended = false;
        seekClockTo(MediaTime::zeroTime());
        {
            Locker locker { m_demuxerLock };
            if (m_demuxer) {
                if (m_audioTrack)
                    mlxweb_media_seek(m_demuxer, m_audioTrack->index, 0);
                if (m_videoTrack)
                    mlxweb_media_seek(m_demuxer, m_videoTrack->index, 0);
            }
        }
        resetDecoders();
    }
    m_playing = true;
    m_audioEnded = false;
    m_videoEnded = false;
    startClock(m_clockBase);
    if (m_audioTrack)
        startAudioThread();
    if (m_videoTrack)
        m_videoTimer.startRepeating(16_ms);
    m_player->playbackStateChanged();
}

void MediaPlayerPrivateMlx::pause()
{
    if (!m_playing)
        return;
    m_playing = false;
    stopClock();
    m_videoTimer.stop();
    wakeAudio();
    m_player->playbackStateChanged();
}

void MediaPlayerPrivateMlx::setRate(float rate)
{
    if (rate <= 0)
        rate = 1;
    stopClock();
    m_rate = rate;
    if (m_playing)
        startClock(m_clockBase);
}

void MediaPlayerPrivateMlx::setVolume(float volume)
{
    m_volume = volume;
    if (m_audioOutput)
        mlxweb_audio_set_volume(m_audioOutput, static_cast<uint32_t>((m_muted ? 0 : m_volume) * 65536));
}

void MediaPlayerPrivateMlx::setMuted(bool muted)
{
    m_muted = muted;
    setVolume(m_volume);
}

Ref<MediaTimePromise> MediaPlayerPrivateMlx::seekToTarget(const SeekTarget& target)
{
    auto time = target.time;
    m_ended = false;
    m_audioEnded = false;
    m_videoEnded = false;
    {
        Locker locker { m_demuxerLock };
        if (m_demuxer) {
            if (m_audioTrack)
                mlxweb_media_seek(m_demuxer, m_audioTrack->index, static_cast<uint64_t>(std::max<int64_t>(0, time.toTimeScale(m_audioTrack->timescale).timeValue())) + m_audioTrack->editMediaTime);
            if (m_videoTrack)
                mlxweb_media_seek(m_demuxer, m_videoTrack->index, static_cast<uint64_t>(std::max<int64_t>(0, time.toTimeScale(m_videoTrack->timescale).timeValue())) + m_videoTrack->editMediaTime);
        }
    }
    flushQueues();
    resetDecoders();
    seekClockTo(time);
    m_lastVideoTime = MediaTime::invalidTime();
    wakeAudio();
#if ENABLE(MEDIA_SOURCE)
    if (m_mediaSource) {
        return m_mediaSource->waitForTarget(target)->whenSettled(RunLoop::mainSingleton(), [weakThis = WeakPtr { *this }](auto&& result) -> Ref<MediaTimePromise> {
            if (!result)
                return MediaTimePromise::createAndReject(result.error());
            RefPtr protectedThis = weakThis.get();
            if (!protectedThis || !protectedThis->m_mediaSource)
                return MediaTimePromise::createAndReject(PlatformMediaError::Cancelled);
            auto seekTime = result.value();
            protectedThis->seekClockTo(seekTime);
            return protectedThis->m_mediaSource->reenqueueMediaForTime(seekTime)->whenSettled(RunLoop::mainSingleton(), [seekTime](auto&&) -> Ref<MediaTimePromise> {
                return MediaTimePromise::createAndResolve(seekTime);
            });
        });
    }
#endif
    m_player->timeChanged();
    return MediaTimePromise::createAndResolve(time);
}

void MediaPlayerPrivateMlx::flushQueues()
{
#if ENABLE(MEDIA_SOURCE)
    Locker locker { m_queueLock };
    for (auto& queue : m_queues.values()) {
        queue.samples.clear();
        queue.ended = false;
    }
#endif
}

void MediaPlayerPrivateMlx::resetDecoders()
{
    if (m_aacDecoder)
        mlxweb_media_aac_reset(m_aacDecoder);
    if (m_h264) {
        mlxweb_media_h264_flush(m_h264);
        uint32_t slot = 0;
        int32_t poc = 0;
        while (mlxweb_media_h264_next_output(m_h264, &slot, &poc)) { }
    }
}

void MediaPlayerPrivateMlx::checkEnded()
{
    if (!m_playing)
        return;
    bool audioDone = !m_audioTrack || m_audioEnded;
    bool videoDone = !m_videoTrack || m_videoEnded;
    if (audioDone && videoDone)
        didReachEnd();
}

void MediaPlayerPrivateMlx::didReachEnd()
{
    m_playing = false;
    m_ended = true;
    stopClock();
    m_videoTimer.stop();
    auto total = duration();
    if (total.isValid() && !total.isPositiveInfinite())
        m_clockBase = total;
    m_player->timeChanged();
    m_player->playbackStateChanged();
}

// ---- Sound ----

void MediaPlayerPrivateMlx::wakeAudio()
{
    Locker locker { m_audioLock };
    m_audioCondition.notifyAll();
}

void MediaPlayerPrivateMlx::startAudioThread()
{
    if (m_audioThread) {
        wakeAudio();
        return;
    }
    m_audioThreadShouldExit = false;
    m_audioThread = Thread::create("MediaPlayerPrivateMlx audio"_s, [this] {
        audioLoop();
    });
}

void MediaPlayerPrivateMlx::stopAudioThread()
{
    if (!m_audioThread)
        return;
    {
        Locker locker { m_audioLock };
        m_audioThreadShouldExit = true;
        m_audioCondition.notifyAll();
    }
    m_audioThread->waitForCompletion();
    m_audioThread = nullptr;
}

bool MediaPlayerPrivateMlx::nextAudioSample(Vector<uint8_t>& bytes, MediaTime& presentationTime)
{
#if ENABLE(MEDIA_SOURCE)
    if (m_mediaSource) {
        RefPtr<MediaSampleMlx> sample;
        WeakPtr<SourceBufferPrivateMlx> waiting;
        bool ended = false;
        {
            Locker locker { m_queueLock };
            auto it = m_queues.find(m_audioTrack->id);
            if (it != m_queues.end()) {
                if (!it->value.samples.isEmpty())
                    sample = it->value.samples.takeFirst();
                ended = it->value.ended && it->value.samples.isEmpty();
                if (it->value.samples.size() < audioQueueLimit / 2 && it->value.waiting) {
                    waiting = WTF::move(it->value.waiting);
                    it->value.waiting = nullptr;
                }
            }
        }
        if (waiting) {
            callOnMainThread([waiting, trackID = m_audioTrack->id] {
                if (RefPtr buffer = waiting.get())
                    buffer->readyForMoreSamples(trackID);
            });
        }
        if (!sample) {
            if (ended)
                m_audioEnded = true;
            return false;
        }
        bytes = sample->bytes();
        presentationTime = sample->presentationTime();
        return true;
    }
#endif
    Locker locker { m_demuxerLock };
    if (!m_demuxer)
        return false;
    mlxweb_media_sample sample;
    if (!mlxweb_media_next_sample(m_demuxer, m_audioTrack->index, &sample)) {
        m_audioEnded = true;
        return false;
    }
    if (sample.offset + sample.size > m_fileBytes.size())
        return false;
    bytes = Vector<uint8_t>(m_fileBytes.subspan(sample.offset, sample.size));
    presentationTime = MediaTime(sample.pts - static_cast<int64_t>(m_audioTrack->editMediaTime), m_audioTrack->timescale);
    return true;
}

void MediaPlayerPrivateMlx::audioLoop()
{
    Vector<float> pcm(8 * 1024);
    Vector<uint8_t> bytes;
    MediaTime presentationTime;
    while (true) {
        {
            Locker locker { m_audioLock };
            while (!m_audioThreadShouldExit && !m_playing)
                m_audioCondition.wait(m_audioLock);
            if (m_audioThreadShouldExit)
                break;
        }
        if (!nextAudioSample(bytes, presentationTime)) {
            if (m_audioEnded) {
                callOnMainThread([weakThis = WeakPtr { *this }] {
                    if (RefPtr protectedThis = weakThis.get())
                        protectedThis->checkEnded();
                });
            }
            m_audioStarved = true;
            Locker locker { m_audioLock };
            m_audioCondition.waitFor(m_audioLock, 20_ms);
            continue;
        }
        if (m_audioStarved) {
            // Playback resumes from the sample's own time.
            m_audioStarved = false;
            m_clockBase = presentationTime;
            m_framesAtClockBase = m_framesWritten.load();
        }
        auto channels = mlxweb_media_aac_decode(m_aacDecoder, bytes.span().data(), bytes.size(), pcm.mutableSpan().data());
        if (!channels)
            continue;
        if (!m_audioOutput) {
            m_audioRate = mlxweb_media_aac_rate(m_aacDecoder);
            m_audioChannels = channels;
            m_audioOutput = mlxweb_audio_open(m_audioRate, m_audioChannels, audioLatencyMilliseconds);
            if (!m_audioOutput) {
                // No sound server: the clock runs by the wall, and the
                // samples are consumed at their rate.
                m_audioStarved = false;
                m_audioEnded = false;
                Locker locker { m_audioLock };
                m_audioCondition.waitFor(m_audioLock, Seconds(1024.0 / std::max<uint32_t>(m_audioRate, 1)));
                continue;
            }
            mlxweb_audio_set_volume(m_audioOutput, static_cast<uint32_t>((m_muted ? 0 : m_volume) * 65536));
        }
        if (channels != m_audioChannels)
            continue;
        if (!mlxweb_audio_write(m_audioOutput, pcm.span().data(), 1024)) {
            mlxweb_audio_close(m_audioOutput);
            m_audioOutput = 0;
            continue;
        }
        m_framesWritten += 1024;
    }
}

// ---- Video ----

bool MediaPlayerPrivateMlx::nextVideoSample(Vector<uint8_t>& bytes, MediaTime& presentationTime, bool& sync)
{
#if ENABLE(MEDIA_SOURCE)
    if (m_mediaSource) {
        RefPtr<MediaSampleMlx> sample;
        WeakPtr<SourceBufferPrivateMlx> waiting;
        bool ended = false;
        {
            Locker locker { m_queueLock };
            auto it = m_queues.find(m_videoTrack->id);
            if (it != m_queues.end()) {
                if (!it->value.samples.isEmpty()) {
                    auto& first = it->value.samples.first();
                    // Only what is due by the clock.
                    if (first->presentationTime() > clockTime() + MediaTime(1, 30))
                        return false;
                    sample = it->value.samples.takeFirst();
                }
                ended = it->value.ended && it->value.samples.isEmpty();
                if (it->value.samples.size() < videoQueueLimit / 2 && it->value.waiting) {
                    waiting = WTF::move(it->value.waiting);
                    it->value.waiting = nullptr;
                }
            }
        }
        if (waiting) {
            if (RefPtr buffer = waiting.get())
                buffer->readyForMoreSamples(m_videoTrack->id);
        }
        if (!sample) {
            if (ended)
                m_videoEnded = true;
            return false;
        }
        bytes = sample->bytes();
        presentationTime = sample->presentationTime();
        sync = sample->flags() & MediaSample::IsSync;
        return true;
    }
#endif
    Locker locker { m_demuxerLock };
    if (!m_demuxer)
        return false;
    mlxweb_media_sample sample;
    if (!mlxweb_media_next_sample(m_demuxer, m_videoTrack->index, &sample)) {
        m_videoEnded = true;
        return false;
    }
    if (sample.offset + sample.size > m_fileBytes.size())
        return false;
    bytes = Vector<uint8_t>(m_fileBytes.subspan(sample.offset, sample.size));
    presentationTime = MediaTime(sample.pts - static_cast<int64_t>(m_videoTrack->editMediaTime), m_videoTrack->timescale);
    sync = sample.sync;
    return true;
}

void MediaPlayerPrivateMlx::videoTick()
{
    if (!m_playing || !m_videoTrack)
        return;
    // A file's samples are taken as they come due; a media source's are
    // checked against the clock before they are taken.
    for (unsigned taken = 0; taken < 4; ++taken) {
        Vector<uint8_t> bytes;
        MediaTime presentationTime;
        bool sync = false;
#if ENABLE(MEDIA_SOURCE)
        if (!m_mediaSource) {
#endif
            if (m_lastVideoTime.isValid() && m_lastVideoTime > clockTime() + MediaTime(1, 30))
                break;
#if ENABLE(MEDIA_SOURCE)
        }
#endif
        if (!nextVideoSample(bytes, presentationTime, sync))
            break;
        m_lastVideoTime = presentationTime;
        decodeVideoSample(bytes, presentationTime, sync);
    }
    if (m_videoEnded)
        checkEnded();
}

// The access unit through the H.264 picture order: its picture in the
// decoded picture buffer, the pictures due for output. Their pixels
// come once the port decodes on the GPU (Vulkan video); until then the
// frames are counted and the layer stays as it is.
void MediaPlayerPrivateMlx::decodeVideoSample(const Vector<uint8_t>& bytes, const MediaTime& presentationTime, bool)
{
    if (!m_h264)
        return;
    if (!m_h264Configured) {
        Locker locker { m_demuxerLock };
        if (m_demuxer) {
            for (size_t set = 0;; ++set) {
                size_t length = 0;
                auto* parameterSet = mlxweb_media_track_parameter_set(m_demuxer, m_videoTrack->index, set, &length);
                if (!parameterSet)
                    break;
                mlxweb_media_h264_parameter_set(m_h264, parameterSet, length);
                m_h264Configured = true;
            }
        }
    }
    auto status = mlxweb_media_h264_access_unit(m_h264, bytes.span().data(), bytes.size(), 4);
    if (status != MLXWEB_MEDIA_OK)
        return;
    mlxweb_media_h264_finish_picture(m_h264);
    uint32_t slot = 0;
    int32_t poc = 0;
    while (mlxweb_media_h264_next_output(m_h264, &slot, &poc)) {
        if (presentationTime + MediaTime(1, 2) < clockTime())
            ++m_droppedFrames;
        else
            ++m_decodedFrames;
    }
}

PlatformLayer* MediaPlayerPrivateMlx::platformLayer() const
{
#if USE(COORDINATED_GRAPHICS)
    return m_layerProxy.get();
#else
    return nullptr;
#endif
}

void MediaPlayerPrivateMlx::paint(GraphicsContext& context, const FloatRect& rect)
{
    if (!m_videoTrack)
        return;
    context.fillRect(rect, Color::black);
}

DestinationColorSpace MediaPlayerPrivateMlx::colorSpace()
{
    return DestinationColorSpace::SRGB();
}

} // namespace WebCore

#endif // ENABLE(VIDEO)
