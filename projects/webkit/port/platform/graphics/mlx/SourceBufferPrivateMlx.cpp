// The Mlx port's SourceBufferPrivate: see SourceBufferPrivateMlx.h.
#include "config.h"
#include "SourceBufferPrivateMlx.h"

#if ENABLE(VIDEO) && ENABLE(MEDIA_SOURCE)

#include "MediaPlayerPrivateMlx.h"
#include "MediaSampleMlx.h"
#include "MediaSourcePrivateMlx.h"
#include "SharedBuffer.h"
#include "SourceBufferPrivateClient.h"
#include "port/MlxWebPort.h"
#include <wtf/text/MakeString.h>

namespace WebCore {

Ref<SourceBufferPrivateMlx> SourceBufferPrivateMlx::create(MediaSourcePrivateMlx& mediaSource, const ContentType& contentType)
{
    return adoptRef(*new SourceBufferPrivateMlx(mediaSource, contentType));
}

SourceBufferPrivateMlx::SourceBufferPrivateMlx(MediaSourcePrivateMlx& mediaSource, const ContentType& contentType)
    : SourceBufferPrivate(mediaSource)
    , m_contentType(contentType)
    , m_logger(Logger::create(this))
    , m_logIdentifier(mediaSource.playerMlx() ? mediaSource.playerMlx()->nextLogIdentifier() : 0)
{
}

SourceBufferPrivateMlx::~SourceBufferPrivateMlx()
{
    if (m_demuxer)
        mlxweb_media_demuxer_destroy(m_demuxer);
}

MediaPlayerPrivateMlx* SourceBufferPrivateMlx::player() const
{
    RefPtr mediaSource = m_mediaSource.get();
    if (!mediaSource)
        return nullptr;
    return static_cast<MediaSourcePrivateMlx&>(*mediaSource).playerMlx();
}

Ref<MediaPromise> SourceBufferPrivateMlx::appendInternal(Ref<SharedBuffer>&& buffer)
{
    if (!m_demuxer)
        m_demuxer = mlxweb_media_demuxer_create();
    if (!m_demuxer)
        return MediaPromise::createAndReject(PlatformMediaError::MemoryError);
    m_pending.append(buffer->span());
    size_t used = 0;
    auto status = mlxweb_media_demuxer_parse(m_demuxer, m_pending.span().data(), m_pending.size(), &used);
    if (status == MLXWEB_MEDIA_CORRUPT || status == MLXWEB_MEDIA_UNSUPPORTED || status == MLXWEB_MEDIA_NO_MEMORY) {
        m_pending.clear();
        mlxweb_media_clear_samples(m_demuxer);
        return MediaPromise::createAndReject(status == MLXWEB_MEDIA_NO_MEMORY ? PlatformMediaError::MemoryError : PlatformMediaError::ParsingError);
    }
    if (!m_haveTracks && mlxweb_media_track_count(m_demuxer)) {
        if (!processInitialization()) {
            m_pending.clear();
            mlxweb_media_clear_samples(m_demuxer);
            return MediaPromise::createAndReject(PlatformMediaError::ParsingError);
        }
    }
    // The samples' offsets count from the bytes parsed: taken before the
    // whole boxes are removed (a box cut short waits for the next append).
    bool samplesOk = processSamples();
    mlxweb_media_clear_samples(m_demuxer);
    if (used)
        m_pending.removeAt(0, used);
    if (!samplesOk)
        return MediaPromise::createAndReject(PlatformMediaError::ParsingError);
    return MediaPromise::createAndResolve();
}

// The tracks of the initialization segment: to the player (their
// decoders) and to WebCore.
bool SourceBufferPrivateMlx::processInitialization()
{
    RefPtr player = this->player();
    if (!player)
        return false;
    size_t count = mlxweb_media_track_count(m_demuxer);
    InitializationSegment segment;
    MediaTime duration = MediaTime::zeroTime();
    m_tracks.clear();
    for (size_t index = 0; index < count; ++index) {
        auto kind = mlxweb_media_track_kind(m_demuxer, index);
        auto codec = mlxweb_media_track_codec(m_demuxer, index);
        Track track;
        track.index = index;
        track.timescale = mlxweb_media_track_timescale(m_demuxer, index);
        track.editMediaTime = mlxweb_media_track_edit_media_time(m_demuxer, index);
        if (!track.timescale)
            continue;
        MediaTime trackDuration(static_cast<int64_t>(mlxweb_media_track_duration(m_demuxer, index)), track.timescale);
        if (trackDuration > duration)
            duration = trackDuration;
        size_t configLength = 0;
        auto* config = mlxweb_media_track_config(m_demuxer, index, &configLength);
        std::span<const uint8_t> configSpan { config, config ? configLength : 0 };
        if (kind == MLXWEB_MEDIA_VIDEO && codec == MLXWEB_MEDIA_H264) {
            track.isVideo = true;
            track.size = FloatSize(mlxweb_media_track_width(m_demuxer, index), mlxweb_media_track_height(m_demuxer, index));
            track.id = player->allocateTrackID();
            Vector<Vector<uint8_t>> parameterSets;
            for (size_t set = 0;; ++set) {
                size_t length = 0;
                auto* bytes = mlxweb_media_track_parameter_set(m_demuxer, index, set, &length);
                if (!bytes)
                    break;
                parameterSets.append(Vector<uint8_t>(std::span<const uint8_t> { bytes, length }));
            }
            player->addSourceTrack(track.id, true, track.timescale, track.editMediaTime, mlxweb_media_track_nal_length_size(m_demuxer, index), configSpan, parameterSets, track.size);
            InitializationSegment::VideoTrackInformation information;
            information.description = MediaDescriptionMlx::create("avc1"_s, true);
            information.track = VideoTrackPrivateMlx::create(track.id, static_cast<int>(m_tracks.size()));
            segment.videoTracks.append(WTF::move(information));
            m_tracks.append(track);
        } else if (kind == MLXWEB_MEDIA_AUDIO && codec == MLXWEB_MEDIA_AAC) {
            track.id = player->allocateTrackID();
            player->addSourceTrack(track.id, false, track.timescale, track.editMediaTime, 0, configSpan, { }, { });
            InitializationSegment::AudioTrackInformation information;
            information.description = MediaDescriptionMlx::create("mp4a.40.2"_s, false);
            information.track = AudioTrackPrivateMlx::create(track.id, static_cast<int>(m_tracks.size()));
            segment.audioTracks.append(WTF::move(information));
            m_tracks.append(track);
        }
    }
    if (m_tracks.isEmpty())
        return false;
    segment.duration = duration;
    m_haveTracks = true;
    didReceiveInitializationSegment(WTF::move(segment));
    return true;
}

// The samples of the media segments parsed, each as a MediaSampleMlx
// with its bytes copied out of the segment.
bool SourceBufferPrivateMlx::processSamples()
{
    size_t count = mlxweb_media_sample_count(m_demuxer);
    if (!count)
        return true;
    if (!m_haveTracks)
        return false;
    for (size_t index = 0; index < count; ++index) {
        mlxweb_media_sample sample;
        if (!mlxweb_media_sample_at(m_demuxer, index, &sample))
            return false;
        const Track* track = nullptr;
        for (auto& candidate : m_tracks) {
            if (candidate.index == sample.track)
                track = &candidate;
        }
        if (!track)
            continue;
        if (sample.offset + sample.size > m_pending.size())
            return false;
        auto bytes = Vector<uint8_t>(m_pending.subspan(sample.offset, sample.size));
        MediaTime presentationTime(sample.pts - static_cast<int64_t>(track->editMediaTime), track->timescale);
        MediaTime decodeTime(static_cast<int64_t>(sample.dts) - static_cast<int64_t>(track->editMediaTime), track->timescale);
        MediaTime duration(static_cast<int64_t>(sample.duration), track->timescale);
        didReceiveSample(MediaSampleMlx::create(WTF::move(bytes), presentationTime, decodeTime, duration, track->id, sample.sync, track->size));
    }
    return true;
}

void SourceBufferPrivateMlx::resetParserStateInternal()
{
    m_pending.clear();
    if (m_demuxer)
        mlxweb_media_clear_samples(m_demuxer);
}

void SourceBufferPrivateMlx::removedFromMediaSource()
{
    if (RefPtr player = this->player()) {
        for (auto& track : m_tracks)
            player->flushTrack(track.id);
    }
    SourceBufferPrivate::removedFromMediaSource();
}

void SourceBufferPrivateMlx::flush(TrackID trackID)
{
    if (RefPtr player = this->player())
        player->flushTrack(trackID);
}

void SourceBufferPrivateMlx::enqueueSample(Ref<MediaSample>&& sample, TrackID trackID)
{
    if (RefPtr player = this->player())
        player->enqueueSample(Ref<MediaSampleMlx> { static_cast<MediaSampleMlx&>(sample.get()) }, trackID);
}

void SourceBufferPrivateMlx::allSamplesInTrackEnqueued(TrackID trackID)
{
    if (RefPtr player = this->player())
        player->setTrackEnded(trackID);
}

bool SourceBufferPrivateMlx::isReadyForMoreSamples(TrackID trackID)
{
    RefPtr player = this->player();
    return player && player->isReadyForMoreSamples(trackID);
}

void SourceBufferPrivateMlx::notifyClientWhenReadyForMoreSamples(TrackID trackID)
{
    if (RefPtr player = this->player())
        player->notifyWhenReadyForMoreSamples(trackID, *this);
}

void SourceBufferPrivateMlx::readyForMoreSamples(TrackID trackID)
{
    provideMediaData(trackID);
}

bool SourceBufferPrivateMlx::canSwitchToType(const ContentType& contentType)
{
    if (!MediaPlayerPrivateMlx::supportsContainer(contentType.containerType()))
        return false;
    for (auto& codec : contentType.codecs()) {
        if (!MediaPlayerPrivateMlx::supportsCodec(codec))
            return false;
    }
    return true;
}

} // namespace WebCore

#endif
