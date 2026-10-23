// Downloads of the Mlx port's embedding (MlxDownloads.h).
#include "config.h"
#include "MlxDownloads.h"

#include "BlobRegistry.h"
#include "BlobRegistryImpl.h"
#include "BlobResourceHandle.h"
#include "DataURLDecoder.h"
#include "MIMETypeRegistry.h"
#include "ResourceError.h"
#include "ResourceRequest.h"
#include "ResourceResponse.h"
#include "port/MlxWebPort.h"
#include "port/embed/MlxNetwork.h"
#include "port/embed/MlxPlatformStrategies.h"
#include <wtf/NeverDestroyed.h>
#include <wtf/RunLoop.h>
#include <wtf/text/CString.h>
#include <wtf/text/MakeString.h>

namespace WebCore {

struct Downloads {
    String directory;
    Vector<MlxDownloads::Download> list;
    Function<void(const MlxDownloads::Download&)> observer;
    unsigned nextId { 1 };
};

static Downloads& downloads()
{
    static NeverDestroyed<Downloads> downloads;
    return downloads;
}

static MlxDownloads::Download* downloadOf(unsigned id)
{
    for (auto& download : downloads().list) {
        if (download.id == id)
            return &download;
    }
    return nullptr;
}

static void changed(const MlxDownloads::Download& download)
{
    if (downloads().observer)
        downloads().observer(download);
}

void MlxDownloads::setDirectory(const String& directory)
{
    downloads().directory = directory;
}

const String& MlxDownloads::directory()
{
    return downloads().directory;
}

const Vector<MlxDownloads::Download>& MlxDownloads::list()
{
    return downloads().list;
}

bool MlxDownloads::running()
{
    for (auto& download : downloads().list) {
        if (download.state == State::Running)
            return true;
    }
    return false;
}

void MlxDownloads::setObserver(Function<void(const Download&)>&& observer)
{
    downloads().observer = WTF::move(observer);
}

static void fail(unsigned id, const String& error)
{
    auto* download = downloadOf(id);
    if (!download || download->state != MlxDownloads::State::Running)
        return;
    download->state = MlxDownloads::State::Failed;
    download->error = error;
    changed(*download);
}

// The bytes written into the download's file (made now, under its name,
// in the directory), and the download done.
static void save(unsigned id, std::span<const uint8_t> bytes)
{
    auto* download = downloadOf(id);
    if (!download || download->state != MlxDownloads::State::Running)
        return;
    if (downloads().directory.isEmpty()) {
        fail(id, "No downloads directory"_s);
        return;
    }
    auto file = mlxweb_download_create(downloads().directory.utf8().data(), download->name.utf8().data());
    if (!file) {
        fail(id, "The file could not be made"_s);
        return;
    }
    download->path = String::fromUTF8(mlxweb_download_path(file));
    download->expected = std::max<uint64_t>(download->expected, bytes.size());
    changed(*download);
    // (In pieces, each noted: the sidebar's progress.)
    constexpr size_t piece = 1 << 20;
    bool written = true;
    for (size_t at = 0; at < bytes.size() && written; at += piece) {
        auto part = bytes.subspan(at, std::min(piece, bytes.size() - at));
        written = mlxweb_download_write(file, part.data(), part.size());
        if (written) {
            download->received += part.size();
            changed(*download);
        }
    }
    bool kept = mlxweb_download_finish(file, written);
    if (!kept) {
        download->path = String();
        fail(id, "The file could not be written"_s);
        return;
    }
    download->state = MlxDownloads::State::Done;
    changed(*download);
}

// The name a download is saved under: the page's, else the response's
// (Content-Disposition's filename, else the URL's last part), else
// "download", with an extension of the type when it has none.
static String nameFor(const URL& url, const ResourceResponse* response, const String& suggestedName)
{
    String name = ResourceResponse::sanitizeSuggestedFilename(suggestedName);
    if (name.isEmpty() && response)
        name = ResourceResponse::sanitizeSuggestedFilename(response->suggestedFilename());
    if (name.isEmpty() && !url.protocolIsData() && !url.protocolIsBlob())
        name = ResourceResponse::sanitizeSuggestedFilename(url.lastPathComponent().toString());
    if (name.isEmpty())
        name = "download"_s;
    if (!name.contains('.') && response) {
        String extension = MIMETypeRegistry::preferredExtensionForMIMEType(response->mimeType());
        if (!extension.isEmpty())
            name = makeString(name, '.', extension);
    }
    return name;
}

void MlxDownloads::start(const ResourceRequest& request, const ResourceResponse* response, const String& suggestedName)
{
    MlxDownloads::Download download;
    download.id = downloads().nextId++;
    download.url = request.url().string();
    download.name = nameFor(request.url(), response, suggestedName);
    if (response && response->expectedContentLength() > 0)
        download.expected = response->expectedContentLength();
    unsigned id = download.id;
    downloads().list.append(WTF::move(download));
    changed(downloads().list.last());

    const URL& url = request.url();
    if (url.protocolIsData()) {
        auto decoded = DataURLDecoder::decode(url);
        if (!decoded) {
            fail(id, "The data: URL is not one"_s);
            return;
        }
        if (!response) {
            auto* started = downloadOf(id);
            if (started && !started->name.contains('.')) {
                String extension = MIMETypeRegistry::preferredExtensionForMIMEType(decoded->mimeType);
                if (!extension.isEmpty())
                    started->name = makeString(started->name, '.', extension);
            }
        }
        save(id, decoded->data.span());
        return;
    }
    if (url.protocolIsBlob()) {
        auto* data = MlxPlatformStrategies::blobData(url);
        if (!data) {
            fail(id, "The blob is gone"_s);
            return;
        }
        ResourceError error;
        ResourceResponse blobResponse;
        Vector<uint8_t> bytes;
        BlobResourceHandle::loadResourceSynchronously(data, request, error, blobResponse, bytes);
        if (!error.isNull()) {
            fail(id, error.localizedDescription());
            return;
        }
        save(id, bytes.span());
        return;
    }
    if (url.protocolIsFile()) {
        uint32_t reason = 0;
        size_t contents = mlxweb_file_read(url.fileSystemPath().utf8().data(), &reason);
        if (!contents) {
            fail(id, reason == MLXWEB_FILE_NOT_FOUND ? "The file does not exist"_s : "The file cannot be read"_s);
            return;
        }
        save(id, std::span<const uint8_t>(mlxweb_file_bytes(contents), mlxweb_file_length(contents)));
        mlxweb_file_release(contents);
        return;
    }
    if (!MlxNetwork::handles(url)) {
        fail(id, "The URL is not one the port downloads"_s);
        return;
    }
    ResourceRequest fetched = request;
    fetched.setHTTPMethod("GET"_s);
    fetched.setHTTPBody(nullptr);
    MlxNetwork::fetch(fetched, [id](const ResourceError& error, ResourceResponse&& response, Vector<uint8_t>&& body) {
        if (!error.isNull()) {
            fail(id, error.localizedDescription());
            return;
        }
        if (response.httpStatusCode() >= 400) {
            fail(id, makeString("The server answered "_s, response.httpStatusCode()));
            return;
        }
        save(id, body.span());
    });
}

} // namespace WebCore
