// Downloads of the Mlx port's embedding (MlxDownloads.h).
#include "config.h"
#include "MlxDownloads.h"

#include "BlobRegistry.h"
#include "BlobRegistryImpl.h"
#include "BlobResourceHandle.h"
#include "DataURLDecoder.h"
#include "HTTPHeaderNames.h"
#include "MIMETypeRegistry.h"
#include "ResourceError.h"
#include "ResourceLoader.h"
#include "ResourceRequest.h"
#include "ResourceResponse.h"
#include "SQLiteDatabase.h"
#include "SQLiteStatement.h"
#include "port/MlxWebPort.h"
#include "port/embed/MlxNetwork.h"
#include "port/embed/MlxPlatformStrategies.h"
#include <atomic>
#include <wtf/FileSystem.h>
#include <wtf/Lock.h>
#include <wtf/MonotonicTime.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/RunLoop.h>
#include <wtf/WallTime.h>
#include <wtf/text/CString.h>
#include <wtf/text/MakeString.h>

namespace WebCore {

namespace {

// A download whose bytes are coming: the network's receiver of them. Its
// partial file is written on the thread that reads the body; the progress
// goes to the main thread a few times a second. pause() and cancel() ask
// the reading to stop; finished() then closes the file as asked.
class Active final : public MlxNetwork::BodyReceiver {
public:
    enum class Stop : uint8_t { None, Pause, Cancel };

    static Ref<Active> create(unsigned id, uintptr_t file, uint64_t received) { return adoptRef(*new Active(id, file, received)); }

    ~Active()
    {
        if (m_file)
            mlxweb_download_release(m_file);
    }

    bool receive(std::span<const uint8_t> bytes) final
    {
        if (m_stop.load() != Stop::None)
            return false;
        if (!mlxweb_download_write(m_file, bytes.data(), bytes.size())) {
            m_writeFailed = true;
            return false;
        }
        uint64_t received = m_received += bytes.size();
        // (The main thread hears of it every tenth of a second at most.)
        int64_t now = static_cast<int64_t>(MonotonicTime::now().secondsSinceEpoch().milliseconds());
        int64_t last = m_noted.load();
        if (now - last >= 100 && m_noted.compare_exchange_strong(last, now)) {
            RunLoop::mainSingleton().dispatch([protectedThis = Ref { *this }, received] {
                protectedThis->progressed(received);
            });
        }
        return true;
    }

    void finished(const ResourceError&, bool stopped) final;

    void stop(Stop stop) { m_stop = stop; }
    Stop stopAsked() const { return m_stop.load(); }
    // The file of a download whose request's response just came (before
    // any byte of its body), with what it has already.
    void begin(uintptr_t file, uint64_t received)
    {
        m_file = file;
        m_received = received;
    }
    unsigned id() const { return m_id; }
    uintptr_t file() const { return m_file; }
    uint64_t received() const { return m_received.load(); }

private:
    Active(unsigned id, uintptr_t file, uint64_t received)
        : m_id(id)
        , m_file(file)
        , m_received(received)
    {
    }

    void progressed(uint64_t received);

    unsigned m_id;
    uintptr_t m_file;
    std::atomic<uint64_t> m_received;
    std::atomic<int64_t> m_noted { 0 };
    std::atomic<Stop> m_stop { Stop::None };
    std::atomic<bool> m_writeFailed { false };
};

// A download's facts beyond the embedder's: the request that goes on with
// it, the validator of a Range request, and its receiver while it runs.
struct Entry {
    MlxDownloads::Download download;
    String method;
    String finalPath;
    String validator;
    RefPtr<Active> active;
    // The rate's last sample.
    MonotonicTime sampled;
    uint64_t sampledBytes { 0 };
};

struct Downloads {
    String directory;
    Vector<Entry> entries;
    // The embedder's view of them (Download facts in the same order).
    Vector<MlxDownloads::Download> list;
    Function<void(const MlxDownloads::Download*)> observer;
    unsigned nextId { 1 };
    std::unique_ptr<SQLiteDatabase> database;
};

}

static Downloads& downloads()
{
    static NeverDestroyed<Downloads> downloads;
    return downloads;
}

static Entry* entryOf(unsigned id)
{
    for (auto& entry : downloads().entries) {
        if (entry.download.id == id)
            return &entry;
    }
    return nullptr;
}

static bool isFinished(MlxDownloads::State state)
{
    return state == MlxDownloads::State::Done || state == MlxDownloads::State::Failed || state == MlxDownloads::State::Cancelled;
}

// The entry written to the profile's database (none open: nothing).
static void store(const Entry& entry)
{
    auto& database = downloads().database;
    if (!database || !database->isOpen())
        return;
    auto statement = database->prepareStatement("INSERT OR REPLACE INTO downloads (id, url, name, path, final, mime, state, received, expected, started, finished, method, validator, error) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"_s);
    if (!statement)
        return;
    auto& d = entry.download;
    statement->bindInt64(1, d.id);
    statement->bindText(2, d.url);
    statement->bindText(3, d.name);
    statement->bindText(4, d.path);
    statement->bindText(5, entry.finalPath);
    statement->bindText(6, d.mimeType);
    statement->bindInt(7, static_cast<int>(d.state));
    statement->bindInt64(8, static_cast<int64_t>(d.received));
    statement->bindInt64(9, static_cast<int64_t>(d.expected));
    statement->bindDouble(10, d.started);
    statement->bindDouble(11, d.finished);
    statement->bindText(12, entry.method);
    statement->bindText(13, entry.validator);
    statement->bindText(14, d.error);
    statement->step();
}

static void forget(unsigned id)
{
    auto& database = downloads().database;
    if (!database || !database->isOpen())
        return;
    if (auto statement = database->prepareStatement("DELETE FROM downloads WHERE id = ?"_s)) {
        statement->bindInt64(1, id);
        statement->step();
    }
}

// The embedder's list made again from the entries, and told.
static void changed(Entry& entry, bool stored = true)
{
    auto& all = downloads();
    all.list.clear();
    for (auto& each : all.entries)
        all.list.append(each.download);
    if (stored)
        store(entry);
    if (all.observer) {
        for (auto& download : all.list) {
            if (download.id == entry.download.id)
                all.observer(&download);
        }
    }
}

static void listChanged()
{
    auto& all = downloads();
    all.list.clear();
    for (auto& each : all.entries)
        all.list.append(each.download);
    if (all.observer)
        all.observer(nullptr);
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

const MlxDownloads::Download* MlxDownloads::find(unsigned id)
{
    for (auto& download : downloads().list) {
        if (download.id == id)
            return &download;
    }
    return nullptr;
}

bool MlxDownloads::running()
{
    for (auto& entry : downloads().entries) {
        if (entry.download.state == State::Running)
            return true;
    }
    return false;
}

void MlxDownloads::setObserver(Function<void(const Download*)>&& observer)
{
    downloads().observer = WTF::move(observer);
}

void MlxDownloads::openDatabase(const String& path)
{
    auto& all = downloads();
    if (path.isEmpty() || all.database)
        return;
    auto database = makeUnique<SQLiteDatabase>();
    FileSystem::makeAllDirectories(FileSystem::parentPath(path));
    if (!database->open(path) || !database->executeCommand("CREATE TABLE IF NOT EXISTS downloads (id INTEGER PRIMARY KEY, url TEXT, name TEXT, path TEXT, final TEXT, mime TEXT, state INTEGER, received INTEGER, expected INTEGER, started REAL, finished REAL, method TEXT, validator TEXT, error TEXT)"_s)) {
        fprintf(stderr, "downloads: cannot open %s\n", path.utf8().data());
        return;
    }
    if (auto statement = database->prepareStatement("SELECT id, url, name, path, final, mime, state, received, expected, started, finished, method, validator, error FROM downloads ORDER BY id"_s)) {
        while (statement->step() == SQLITE_ROW) {
            Entry entry;
            auto& d = entry.download;
            d.id = static_cast<unsigned>(statement->columnInt64(0));
            d.url = statement->columnText(1);
            d.name = statement->columnText(2);
            d.path = statement->columnText(3);
            entry.finalPath = statement->columnText(4);
            d.mimeType = statement->columnText(5);
            d.state = static_cast<State>(std::min(statement->columnInt(6), static_cast<int>(State::Cancelled)));
            d.received = static_cast<uint64_t>(statement->columnInt64(7));
            d.expected = static_cast<uint64_t>(statement->columnInt64(8));
            d.started = statement->columnDouble(9);
            d.finished = statement->columnDouble(10);
            entry.method = statement->columnText(11);
            entry.validator = statement->columnText(12);
            d.error = statement->columnText(13);
            d.resumable = entry.method == "GET"_s;
            // (One that was running when the process ended is paused: its
            // partial file has what came, which it goes on from.)
            if (d.state == State::Running || d.state == State::Paused) {
                d.state = State::Paused;
                auto size = mlxweb_download_size(d.path.utf8().data());
                if (size < 0) {
                    d.state = State::Failed;
                    d.error = "The partial file is gone"_s;
                } else
                    d.received = static_cast<uint64_t>(size);
            }
            all.nextId = std::max(all.nextId, d.id + 1);
            all.entries.append(WTF::move(entry));
        }
    }
    all.database = WTF::move(database);
    for (auto& entry : all.entries)
        store(entry);
    listChanged();
}

static void fail(unsigned id, const String& error)
{
    auto* entry = entryOf(id);
    if (!entry || isFinished(entry->download.state))
        return;
    entry->download.state = MlxDownloads::State::Failed;
    entry->download.error = error;
    entry->download.rate = 0;
    entry->download.finished = WallTime::now().secondsSinceEpoch().seconds();
    changed(*entry);
}

// A new entry, listed, for a download of URL named NAME.
static Entry& add(const URL& url, const String& name, const String& method, const ResourceResponse* response)
{
    Entry entry;
    auto& d = entry.download;
    d.id = downloads().nextId++;
    d.url = url.string();
    d.name = name;
    d.started = WallTime::now().secondsSinceEpoch().seconds();
    d.resumable = method == "GET"_s && url.protocolIsInHTTPFamily();
    entry.method = method;
    if (response) {
        d.mimeType = response->mimeType();
        if (response->expectedContentLength() > 0)
            d.expected = response->expectedContentLength();
    }
    entry.sampled = MonotonicTime::now();
    downloads().entries.append(WTF::move(entry));
    auto& added = downloads().entries.last();
    changed(added);
    return added;
}

// The partial file made for the entry (in the directory, under its
// name); false when it could not be (the download failed).
static bool makeFile(Entry& entry, uintptr_t& file)
{
    file = 0;
    if (downloads().directory.isEmpty()) {
        fail(entry.download.id, "No downloads directory"_s);
        return false;
    }
    file = mlxweb_download_create(downloads().directory.utf8().data(), entry.download.name.utf8().data());
    if (!file) {
        fail(entry.download.id, "The file could not be made"_s);
        return false;
    }
    entry.download.path = String::fromUTF8(mlxweb_download_partial_path(file));
    entry.finalPath = String::fromUTF8(mlxweb_download_path(file));
    entry.download.name = FileSystem::pathFileName(entry.finalPath);
    return true;
}

// The validator of a Range request's If-Range: the response's ETag (a
// strong one), else its Last-Modified.
static String validatorOf(const ResourceResponse& response)
{
    String tag = response.httpHeaderField(HTTPHeaderName::ETag);
    if (!tag.isEmpty() && !tag.startsWith("W/"_s))
        return tag;
    return response.httpHeaderField(HTTPHeaderName::LastModified);
}

void Active::progressed(uint64_t received)
{
    auto* entry = entryOf(m_id);
    if (!entry || entry->active != this || entry->download.state != MlxDownloads::State::Running)
        return;
    entry->download.received = received;
    auto now = MonotonicTime::now();
    auto elapsed = (now - entry->sampled).seconds();
    if (elapsed >= 0.5) {
        double rate = (received - entry->sampledBytes) / elapsed;
        entry->download.rate = entry->download.rate ? 0.7 * entry->download.rate + 0.3 * rate : rate;
        entry->sampled = now;
        entry->sampledBytes = received;
    }
    changed(*entry, false);
}

void Active::finished(const ResourceError& error, bool stopped)
{
    auto* entry = entryOf(m_id);
    uintptr_t file = std::exchange(m_file, 0);
    if (!entry || entry->active != this) {
        if (file) {
            mlxweb_download_finish(file, 0);
            mlxweb_download_release(file);
        }
        return;
    }
    Ref protectedThis { *this };
    entry->active = nullptr;
    auto& d = entry->download;
    d.received = m_received.load();
    d.rate = 0;
    auto stop = m_stop.load();
    if (stopped && stop == Stop::Pause && !m_writeFailed) {
        bool kept = mlxweb_download_finish(file, 1);
        mlxweb_download_release(file);
        d.state = kept ? MlxDownloads::State::Paused : MlxDownloads::State::Failed;
        if (!kept)
            d.error = "The file could not be written"_s;
        changed(*entry);
        return;
    }
    if (stopped && stop == Stop::Cancel) {
        mlxweb_download_finish(file, 0);
        mlxweb_download_release(file);
        d.state = MlxDownloads::State::Cancelled;
        d.path = String();
        d.finished = WallTime::now().secondsSinceEpoch().seconds();
        changed(*entry);
        return;
    }
    if (!error.isNull() || stopped) {
        mlxweb_download_finish(file, 0);
        mlxweb_download_release(file);
        d.path = String();
        fail(d.id, m_writeFailed ? "The file could not be written"_s : error.isNull() ? "The download stopped"_s : error.localizedDescription());
        return;
    }
    bool renamed = mlxweb_download_finish(file, 2);
    String finalPath = String::fromUTF8(mlxweb_download_path(file));
    mlxweb_download_release(file);
    if (!renamed) {
        d.path = String();
        fail(d.id, "The file could not be written"_s);
        return;
    }
    d.path = finalPath;
    d.name = FileSystem::pathFileName(finalPath);
    d.expected = std::max(d.expected, d.received);
    d.state = MlxDownloads::State::Done;
    d.finished = WallTime::now().secondsSinceEpoch().seconds();
    changed(*entry);
}

// Bytes the entry has whole (data:, blob:, file:): written and done.
static void saveWhole(unsigned id, std::span<const uint8_t> bytes)
{
    auto* entry = entryOf(id);
    if (!entry)
        return;
    uintptr_t file;
    if (!makeFile(*entry, file))
        return;
    Ref active = Active::create(id, file, 0);
    entry->active = active.copyRef();
    entry->download.expected = std::max<uint64_t>(entry->download.expected, bytes.size());
    changed(*entry);
    bool written = active->receive(bytes);
    active->finished(written ? ResourceError { } : ResourceError(ResourceError::Type::General), !written);
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

// A request of the entry's own (a link's download attribute, a download
// resumed, RESUMING, with OFFSET bytes kept): its response names the file
// (a new one), or goes on with the partial file (a 206), or starts it
// again.
static void fetchFor(unsigned id, ResourceRequest&& request, bool resuming, uint64_t offset)
{
    auto* entry = entryOf(id);
    if (!entry)
        return;
    Ref active = Active::create(id, 0, 0);
    entry->active = active.copyRef();
    MlxNetwork::fetch(request, [id, resuming, offset, active = active.copyRef()](const ResourceError& error, const ResourceResponse& response) -> bool {
        auto* entry = entryOf(id);
        if (!entry || entry->active != active.ptr())
            return false;
        auto& d = entry->download;
        // (Paused or cancelled before the response came.)
        auto stop = active->stopAsked();
        if (stop != Active::Stop::None) {
            entry->active = nullptr;
            d.state = stop == Active::Stop::Pause && resuming ? MlxDownloads::State::Paused : MlxDownloads::State::Cancelled;
            if (d.state == MlxDownloads::State::Cancelled && resuming && !d.path.isEmpty())
                mlxweb_download_remove(d.path.utf8().data());
            if (d.state == MlxDownloads::State::Cancelled)
                d.path = String();
            changed(*entry);
            return false;
        }
        if (!error.isNull()) {
            entry->active = nullptr;
            fail(id, error.localizedDescription());
            return false;
        }
        auto status = response.httpStatusCode();
        if (status >= 400) {
            entry->active = nullptr;
            fail(id, makeString("The server answered "_s, status));
            return false;
        }
        if (d.mimeType.isEmpty())
            d.mimeType = response.mimeType();
        String validator = validatorOf(response);
        uintptr_t file = 0;
        uint64_t received = 0;
        auto length = response.expectedContentLength();
        if (resuming && offset && status == 206) {
            // (Going on: the rest's length after what is kept.)
            file = mlxweb_download_reopen(d.path.utf8().data(), entry->finalPath.utf8().data(), 1);
            received = offset;
            if (length > 0)
                d.expected = offset + length;
        } else if (resuming) {
            // (A server that answers whole: from the start.)
            file = mlxweb_download_reopen(d.path.utf8().data(), entry->finalPath.utf8().data(), 0);
            if (length > 0)
                d.expected = length;
        } else {
            d.name = nameFor(response.url(), &response, d.name);
            if (length > 0)
                d.expected = length;
            if (!makeFile(*entry, file)) {
                entry->active = nullptr;
                return false;
            }
        }
        if (!file) {
            entry->active = nullptr;
            fail(id, "The partial file could not be opened"_s);
            return false;
        }
        entry->validator = validator;
        active->begin(file, received);
        d.received = received;
        d.state = MlxDownloads::State::Running;
        entry->sampled = MonotonicTime::now();
        entry->sampledBytes = received;
        changed(*entry);
        return true;
    }, active.copyRef());
}

void MlxDownloads::start(const ResourceRequest& request, const ResourceResponse* response, const String& suggestedName)
{
    const URL& url = request.url();
    String method = request.httpMethod().isEmpty() ? "GET"_s : request.httpMethod();
    auto& entry = add(url, nameFor(url, response, suggestedName), method, response);
    unsigned id = entry.download.id;

    if (url.protocolIsData()) {
        auto decoded = DataURLDecoder::decode(url);
        if (!decoded) {
            fail(id, "The data: URL is not one"_s);
            return;
        }
        if (!response) {
            if (!entry.download.name.contains('.')) {
                String extension = MIMETypeRegistry::preferredExtensionForMIMEType(decoded->mimeType);
                if (!extension.isEmpty())
                    entry.download.name = makeString(entry.download.name, '.', extension);
            }
            entry.download.mimeType = decoded->mimeType;
        }
        saveWhole(id, decoded->data.span());
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
        saveWhole(id, bytes.span());
        return;
    }
    if (url.protocolIsFile()) {
        uint32_t reason = 0;
        size_t contents = mlxweb_file_read(url.fileSystemPath().utf8().data(), &reason);
        if (!contents) {
            fail(id, reason == MLXWEB_FILE_NOT_FOUND ? "The file does not exist"_s : "The file cannot be read"_s);
            return;
        }
        saveWhole(id, std::span<const uint8_t>(mlxweb_file_bytes(contents), mlxweb_file_length(contents)));
        mlxweb_file_release(contents);
        return;
    }
    if (!MlxNetwork::handles(url)) {
        fail(id, "The URL is not one the port downloads"_s);
        return;
    }
    // (The name the response gives is the one the file takes.)
    if (suggestedName.isEmpty())
        entry.download.name = String();
    else
        entry.download.name = ResourceResponse::sanitizeSuggestedFilename(suggestedName);
    fetchFor(id, ResourceRequest { request }, false, 0);
}

void MlxDownloads::convert(ResourceLoader* loader, const ResourceRequest& request, const ResourceResponse& response, const String& suggestedName)
{
    if (!loader || !MlxNetwork::handles(response.url().isEmpty() ? request.url() : response.url())) {
        start(request, &response, suggestedName);
        return;
    }
    URL url = response.url().isEmpty() ? request.url() : response.url();
    String method = request.httpMethod().isEmpty() ? "GET"_s : request.httpMethod();
    auto& entry = add(url, nameFor(url, &response, suggestedName), method, &response);
    unsigned id = entry.download.id;
    entry.validator = validatorOf(response);
    uintptr_t file;
    if (!makeFile(entry, file))
        return;
    Ref active = Active::create(id, file, 0);
    entry.active = active.copyRef();
    changed(entry);
    if (!MlxNetwork::divert(*loader, WTF::move(active))) {
        // (Its body was taken already: read again, as a new download.)
        auto* failed = entryOf(id);
        if (failed && failed->active) {
            uintptr_t made = failed->active->file();
            failed->active = nullptr;
            mlxweb_download_finish(made, 0);
        }
        downloads().entries.removeFirstMatching([id](auto& each) { return each.download.id == id; });
        forget(id);
        listChanged();
        start(request, &response, suggestedName);
    }
}

void MlxDownloads::pause(unsigned id)
{
    auto* entry = entryOf(id);
    if (!entry || entry->download.state != State::Running)
        return;
    if (!entry->download.resumable) {
        cancel(id);
        return;
    }
    if (entry->active)
        entry->active->stop(Active::Stop::Pause);
}

void MlxDownloads::resume(unsigned id)
{
    auto* entry = entryOf(id);
    if (!entry || entry->download.state != State::Paused || entry->active)
        return;
    auto& d = entry->download;
    ResourceRequest request { URL { d.url } };
    request.setHTTPMethod("GET"_s);
    uint64_t offset = d.received;
    if (offset) {
        request.setHTTPHeaderField(HTTPHeaderName::Range, makeString("bytes="_s, offset, '-'));
        if (!entry->validator.isEmpty())
            request.setHTTPHeaderField(HTTPHeaderName::IfRange, entry->validator);
    }
    d.state = State::Running;
    d.error = String();
    changed(*entry);
    fetchFor(id, WTF::move(request), true, offset);
}

void MlxDownloads::cancel(unsigned id)
{
    auto* entry = entryOf(id);
    if (!entry || isFinished(entry->download.state))
        return;
    if (entry->download.state == State::Running && entry->active && entry->active->file()) {
        entry->active->stop(Active::Stop::Cancel);
        return;
    }
    // (Paused, or not started writing: its partial file removed now.)
    entry->active = nullptr;
    if (!entry->download.path.isEmpty())
        mlxweb_download_remove(entry->download.path.utf8().data());
    entry->download.path = String();
    entry->download.state = State::Cancelled;
    entry->download.rate = 0;
    entry->download.finished = WallTime::now().secondsSinceEpoch().seconds();
    changed(*entry);
}

void MlxDownloads::clearFinished()
{
    auto& all = downloads();
    Vector<unsigned> cleared;
    for (auto& entry : all.entries) {
        if (isFinished(entry.download.state))
            cleared.append(entry.download.id);
    }
    if (cleared.isEmpty())
        return;
    all.entries.removeAllMatching([](auto& entry) { return isFinished(entry.download.state); });
    for (auto id : cleared)
        forget(id);
    listChanged();
}

} // namespace WebCore
