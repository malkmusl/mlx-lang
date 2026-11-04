// The media samples, tracks and descriptions of the Mlx port's media
// player (MediaPlayerPrivateMlx): a sample is its bytes (one MP4 sample:
// an AAC block, an AVC access unit of length-prefixed NAL units) with its
// times in the track's timescale; a track is an MP4 track, its ID the
// file's; a description is the codec string ("avc1.64001f",
// "mp4a.40.2") and whether it is sound or video.
#pragma once

#if ENABLE(VIDEO)

#include "AudioTrackPrivate.h"
#include "FloatSize.h"
#include "MediaDescription.h"
#include "MediaSample.h"
#include "VideoTrackPrivate.h"
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class MediaSampleMlx final : public MediaSample {
public:
    static Ref<MediaSampleMlx> create(Vector<uint8_t>&& bytes, const MediaTime& presentationTime, const MediaTime& decodeTime, const MediaTime& duration, TrackID trackID, bool isSync, const FloatSize& presentationSize)
    {
        return adoptRef(*new MediaSampleMlx(WTF::move(bytes), presentationTime, decodeTime, duration, trackID, isSync, presentationSize));
    }

    const Vector<uint8_t>& bytes() const { return m_bytes; }

    MediaTime presentationTime() const final { return m_presentationTime; }
    MediaTime decodeTime() const final { return m_decodeTime; }
    MediaTime duration() const final { return m_duration; }
    TrackID trackID() const final { return m_trackID; }
    size_t sizeInBytes() const final { return m_bytes.size(); }
    FloatSize presentationSize() const final { return m_presentationSize; }
    void offsetTimestampsBy(const MediaTime& offset) final
    {
        m_presentationTime += offset;
        m_decodeTime += offset;
    }
    void setTimestamps(const MediaTime& presentationTime, const MediaTime& decodeTime) final
    {
        m_presentationTime = presentationTime;
        m_decodeTime = decodeTime;
    }
    Ref<MediaSample> createNonDisplayingCopy() const final
    {
        auto copy = create(Vector<uint8_t>(m_bytes), m_presentationTime, m_decodeTime, m_duration, m_trackID, m_isSync, m_presentationSize);
        copy->m_isNonDisplaying = true;
        return copy;
    }
    SampleFlags flags() const final
    {
        unsigned flags = None;
        if (m_isSync)
            flags |= IsSync;
        if (m_isNonDisplaying)
            flags |= IsNonDisplaying;
        return static_cast<SampleFlags>(flags);
    }
    PlatformSample platformSample() const final { return PlatformSample { static_cast<const MockSampleBox*>(nullptr) }; }
    Type type() const final { return Type::None; }
    void dump(PrintStream& out) const final
    {
        out.print("{PTS(", m_presentationTime, "), DTS(", m_decodeTime, "), duration(", m_duration, "), ", m_bytes.size(), " bytes}");
    }

private:
    MediaSampleMlx(Vector<uint8_t>&& bytes, const MediaTime& presentationTime, const MediaTime& decodeTime, const MediaTime& duration, TrackID trackID, bool isSync, const FloatSize& presentationSize)
        : m_bytes(WTF::move(bytes))
        , m_presentationTime(presentationTime)
        , m_decodeTime(decodeTime)
        , m_duration(duration)
        , m_trackID(trackID)
        , m_isSync(isSync)
        , m_presentationSize(presentationSize)
    {
    }

    Vector<uint8_t> m_bytes;
    MediaTime m_presentationTime;
    MediaTime m_decodeTime;
    MediaTime m_duration;
    TrackID m_trackID { 0 };
    bool m_isSync { false };
    bool m_isNonDisplaying { false };
    FloatSize m_presentationSize;
};

class AudioTrackPrivateMlx final : public AudioTrackPrivate {
public:
    static Ref<AudioTrackPrivateMlx> create(TrackID id, int index) { return adoptRef(*new AudioTrackPrivateMlx(id, index)); }
    TrackID id() const final { return m_id; }
    int trackIndex() const final { return m_index; }
    Kind kind() const final { return Kind::Main; }

private:
    AudioTrackPrivateMlx(TrackID id, int index)
        : m_id(id)
        , m_index(index)
    {
    }
    TrackID m_id;
    int m_index;
};

class VideoTrackPrivateMlx final : public VideoTrackPrivate {
public:
    static Ref<VideoTrackPrivateMlx> create(TrackID id, int index) { return adoptRef(*new VideoTrackPrivateMlx(id, index)); }
    TrackID id() const final { return m_id; }
    int trackIndex() const final { return m_index; }
    Kind kind() const final { return Kind::Main; }

private:
    VideoTrackPrivateMlx(TrackID id, int index)
        : m_id(id)
        , m_index(index)
    {
    }
    TrackID m_id;
    int m_index;
};

class MediaDescriptionMlx final : public MediaDescription {
public:
    static Ref<MediaDescriptionMlx> create(String&& codec, bool isVideo) { return adoptRef(*new MediaDescriptionMlx(WTF::move(codec), isVideo)); }
    bool isVideo() const final { return m_isVideo; }
    bool isAudio() const final { return !m_isVideo; }
    bool isText() const final { return false; }

private:
    MediaDescriptionMlx(String&& codec, bool isVideo)
        : MediaDescription(WTF::move(codec))
        , m_isVideo(isVideo)
    {
    }
    bool m_isVideo;
};

} // namespace WebCore

#endif // ENABLE(VIDEO)
