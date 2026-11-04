// The Mlx port's MediaSourcePrivate: see MediaSourcePrivateMlx.h.
#include "config.h"
#include "MediaSourcePrivateMlx.h"

#if ENABLE(VIDEO) && ENABLE(MEDIA_SOURCE)

#include "ContentType.h"
#include "MediaPlayerPrivateMlx.h"
#include "MediaSourceConfiguration.h"
#include "SourceBufferPrivateMlx.h"

namespace WebCore {

Ref<MediaSourcePrivateMlx> MediaSourcePrivateMlx::create(MediaSourcePrivateClient& client, MediaPlayerPrivateMlx& player)
{
    return adoptRef(*new MediaSourcePrivateMlx(client, player));
}

MediaSourcePrivateMlx::MediaSourcePrivateMlx(MediaSourcePrivateClient& client, MediaPlayerPrivateMlx& player)
    : MediaSourcePrivate(client)
    , m_player(player)
{
}

MediaSourcePrivateMlx::~MediaSourcePrivateMlx() = default;

RefPtr<MediaPlayerPrivateInterface> MediaSourcePrivateMlx::player() const
{
    return m_player.get();
}

void MediaSourcePrivateMlx::setPlayer(MediaPlayerPrivateInterface* player)
{
    m_player = player ? static_cast<MediaPlayerPrivateMlx*>(player) : nullptr;
}

MediaSourcePrivate::AddStatus MediaSourcePrivateMlx::addSourceBuffer(const ContentType& contentType, const MediaSourceConfiguration&, RefPtr<SourceBufferPrivate>& outPrivate)
{
    auto container = contentType.containerType();
    if (!MediaPlayerPrivateMlx::supportsContainer(container))
        return AddStatus::NotSupported;
    for (auto& codec : contentType.codecs()) {
        if (!MediaPlayerPrivateMlx::supportsCodec(codec))
            return AddStatus::NotSupported;
    }
    auto buffer = SourceBufferPrivateMlx::create(*this, contentType);
    outPrivate = buffer.copyRef();
    return AddStatus::Ok;
}

void MediaSourcePrivateMlx::notifyActiveSourceBuffersChanged()
{
    if (RefPtr player = m_player.get())
        player->notifyActiveSourceBuffersChanged();
}

void MediaSourcePrivateMlx::durationChanged(const MediaTime& duration)
{
    MediaSourcePrivate::durationChanged(duration);
    if (RefPtr player = m_player.get())
        player->mediaSourceDurationChanged();
}

void MediaSourcePrivateMlx::bufferedChanged(PlatformTimeRanges&& ranges)
{
    MediaSourcePrivate::bufferedChanged(WTF::move(ranges));
    if (RefPtr player = m_player.get())
        player->mediaSourceBufferedChanged();
}

void MediaSourcePrivateMlx::markEndOfStream(EndOfStreamStatus status)
{
    MediaSourcePrivate::markEndOfStream(status);
    if (RefPtr player = m_player.get())
        player->mediaSourceHasRetrievedAllData();
}

} // namespace WebCore

#endif
