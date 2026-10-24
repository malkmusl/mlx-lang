// Web storage of the Mlx port's embedding (MlxStorage.h).
#include "config.h"
#include "MlxStorage.h"

#include "Document.h"
#include "LocalDOMWindow.h"
#include "LocalFrame.h"
#include "Page.h"
#include "SQLiteDatabase.h"
#include "SQLiteStatement.h"
#include "SQLiteTransaction.h"
#include "SecurityOrigin.h"
#include "SecurityOriginData.h"
#include "Storage.h"
#include "StorageArea.h"
#include "StorageEventDispatcher.h"
#include "StorageMap.h"
#include "StorageNamespace.h"
#include "StorageType.h"
#include <sqlite3.h>
#include <pal/SessionID.h>
#include <stdio.h>
#include <stdlib.h>
#include <wtf/FileSystem.h>
#include <wtf/HashMap.h>
#include <wtf/RunLoop.h>
#include <wtf/text/CString.h>

namespace WebCore {

// MLX_STORAGE_TRACE in the environment: databases opened and written, on
// standard error.
static bool storageTracing()
{
    static bool trace = getenv("MLX_STORAGE_TRACE");
    return trace;
}

// How long after a change local storage is written (WebKitLegacy's
// StorageAreaSync waited a second too).
static constexpr Seconds syncDelay = 1_s;

class MlxStorageArea;

// The areas of local storage with changes not written yet.
static HashSet<RefPtr<MlxStorageArea>>& unsyncedAreas()
{
    static NeverDestroyed<HashSet<RefPtr<MlxStorageArea>>> areas;
    return areas;
}

static void syncAll();

static void scheduleSync()
{
    static bool scheduled;
    if (scheduled)
        return;
    scheduled = true;
    RunLoop::mainSingleton().dispatchAfter(syncDelay, [] {
        scheduled = false;
        syncAll();
    });
}

class MlxStorageArea final : public StorageArea {
public:
    // PATH: the database of a persistent local storage area (null for one
    // in memory).
    static Ref<MlxStorageArea> create(StorageType type, const SecurityOriginData& origin, unsigned quota, const String& path)
    {
        return adoptRef(*new MlxStorageArea(type, origin, quota, path));
    }

    Ref<MlxStorageArea> copy() const
    {
        Ref area = create(m_type, m_origin, m_map.quota(), String());
        area->m_map = m_map;
        area->m_loaded = true;
        return area;
    }

    // Writes the pending changes in one transaction.
    void sync()
    {
        if (!m_clearPending && m_pending.isEmpty())
            return;
        if (!openDatabase()) {
            m_pending.clear();
            m_clearPending = false;
            return;
        }
        SQLiteTransaction transaction(*m_database);
        transaction.begin();
        if (m_clearPending)
            m_database->executeCommand("DELETE FROM ItemTable"_s);
        for (auto& change : m_pending) {
            if (change.value.isNull()) {
                if (auto statement = m_database->prepareStatement("DELETE FROM ItemTable WHERE key=?"_s)) {
                    statement->bindText(1, change.key);
                    statement->step();
                }
            } else if (auto statement = m_database->prepareStatement("INSERT INTO ItemTable VALUES (?, ?)"_s)) {
                statement->bindText(1, change.key);
                statement->bindBlob(2, change.value);
                if (statement->step() != SQLITE_DONE && storageTracing())
                    fprintf(stderr, "storage: %s: cannot write %s: %s\n", m_path.utf8().data(), change.key.utf8().data(), m_database->lastErrorMsg());
            }
        }
        transaction.commit();
        if (storageTracing())
            fprintf(stderr, "storage: %s: %u changes written%s\n", m_path.utf8().data(), m_pending.size(), m_clearPending ? " after a clear" : "");
        m_pending.clear();
        m_clearPending = false;
    }

private:
    MlxStorageArea(StorageType type, const SecurityOriginData& origin, unsigned quota, const String& path)
        : m_type(type)
        , m_origin(origin)
        , m_map(quota)
        , m_path(path)
    {
    }

    unsigned length() final
    {
        load();
        return m_map.length();
    }

    String key(unsigned index) final
    {
        load();
        return m_map.key(index);
    }

    String item(const String& key) final
    {
        load();
        return m_map.getItem(key);
    }

    bool contains(const String& key) final
    {
        load();
        return m_map.contains(key);
    }

    void setItem(LocalFrame& frame, const String& key, const String& value, bool& quotaException) final
    {
        load();
        String oldValue;
        m_map.setItem(key, value, oldValue, quotaException);
        if (quotaException || oldValue == value)
            return;
        changed(key, value);
        dispatch(frame, key, oldValue, value);
    }

    void removeItem(LocalFrame& frame, const String& key) final
    {
        load();
        String oldValue;
        m_map.removeItem(key, oldValue);
        if (oldValue.isNull())
            return;
        changed(key, String());
        dispatch(frame, key, oldValue, String());
    }

    void clear(LocalFrame& frame) final
    {
        load();
        if (!m_map.length())
            return;
        m_map.clear();
        if (!m_path.isNull()) {
            m_pending.clear();
            m_clearPending = true;
            unsyncedAreas().add(this);
            scheduleSync();
        }
        dispatch(frame, String(), String(), String());
    }

    StorageType storageType() const final { return m_type; }
    size_t memoryBytesUsedByCache() final { return 0; }

    void changed(const String& key, const String& value)
    {
        if (m_path.isNull())
            return;
        m_pending.set(key, value);
        unsyncedAreas().add(this);
        scheduleSync();
    }

    // The `storage` events of a change, at the origin's other windows
    // (those whose Storage is not the changing window's).
    void dispatch(LocalFrame& frame, const String& key, const String& oldValue, const String& newValue)
    {
        RefPtr document = frame.document();
        RefPtr window = frame.window();
        if (!document)
            return;
        Ref origin = document->securityOrigin();
        String url = document->url().string();
        if (m_type == StorageType::Session) {
            RefPtr page = frame.page();
            if (!page)
                return;
            RefPtr source = window ? window->optionalSessionStorage() : nullptr;
            StorageEventDispatcher::dispatchSessionStorageEvents(key, oldValue, newValue, *page, origin, url, [source](Storage& storage) {
                return &storage == source.get();
            });
            return;
        }
        RefPtr source = window ? window->optionalLocalStorage() : nullptr;
        StorageEventDispatcher::dispatchLocalStorageEvents(key, oldValue, newValue, nullptr, origin, url, [source](Storage& storage) {
            return &storage == source.get();
        });
    }

    bool openDatabase()
    {
        if (m_database)
            return m_database->isOpen();
        m_database = makeUnique<SQLiteDatabase>();
        FileSystem::makeAllDirectories(FileSystem::parentPath(m_path));
        if (!m_database->open(m_path)) {
            fprintf(stderr, "storage: cannot open %s: %s\n", m_path.utf8().data(), m_database->lastErrorMsg());
            return false;
        }
        if (!m_database->executeCommand("CREATE TABLE IF NOT EXISTS ItemTable (key TEXT UNIQUE ON CONFLICT REPLACE, value BLOB NOT NULL ON CONFLICT FAIL)"_s)) {
            fprintf(stderr, "storage: cannot make the table of %s: %s\n", m_path.utf8().data(), m_database->lastErrorMsg());
            m_database->close();
            return false;
        }
        return true;
    }

    // The items of the database, the first time.
    void load()
    {
        if (m_loaded)
            return;
        m_loaded = true;
        if (m_path.isNull() || !FileSystem::fileExists(m_path))
            return;
        if (!openDatabase())
            return;
        HashMap<String, String> items;
        if (auto statement = m_database->prepareStatement("SELECT key, value FROM ItemTable"_s)) {
            while (statement->step() == SQLITE_ROW) {
                String key = statement->columnText(0);
                String value = statement->columnBlobAsString(1);
                if (!key.isNull() && !value.isNull())
                    items.set(WTF::move(key), WTF::move(value));
            }
        }
        if (storageTracing())
            fprintf(stderr, "storage: %s: %u items read\n", m_path.utf8().data(), items.size());
        m_map.importItems(WTF::move(items));
    }

    StorageType m_type;
    SecurityOriginData m_origin;
    StorageMap m_map;
    String m_path;
    bool m_loaded { false };
    std::unique_ptr<SQLiteDatabase> m_database;
    // The changes not written yet: a key's value, or null for one removed.
    HashMap<String, String> m_pending;
    bool m_clearPending { false };
};

static void syncAll()
{
    auto areas = std::exchange(unsyncedAreas(), { });
    for (auto& area : areas)
        area->sync();
}

class MlxStorageNamespace final : public StorageNamespace {
public:
    // DIRECTORY: a persistent local storage's (null: in memory).
    static Ref<MlxStorageNamespace> create(StorageType type, unsigned quota, PAL::SessionID sessionID, const String& directory, RefPtr<const SecurityOrigin>&& topLevelOrigin)
    {
        return adoptRef(*new MlxStorageNamespace(type, quota, sessionID, directory, WTF::move(topLevelOrigin)));
    }

private:
    MlxStorageNamespace(StorageType type, unsigned quota, PAL::SessionID sessionID, const String& directory, RefPtr<const SecurityOrigin>&& topLevelOrigin)
        : m_type(type)
        , m_quota(quota)
        , m_sessionID(sessionID)
        , m_directory(directory)
        , m_topLevelOrigin(WTF::move(topLevelOrigin))
    {
    }

    Ref<StorageArea> storageArea(const SecurityOrigin& origin) final
    {
        return m_areas.ensure(origin.data(), [&] {
            String path;
            if (!m_directory.isNull())
                path = FileSystem::pathByAppendingComponent(m_directory, makeString(origin.data().databaseIdentifier(), ".localstorage"_s));
            return MlxStorageArea::create(m_type, origin.data(), m_quota, path);
        }).iterator->value;
    }

    const SecurityOrigin* topLevelOrigin() const final { return m_topLevelOrigin.get(); }

    Ref<StorageNamespace> copy(Page&) final
    {
        Ref copy = create(m_type, m_quota, m_sessionID, String(), RefPtr { m_topLevelOrigin });
        for (auto& entry : m_areas)
            copy->m_areas.set(entry.key, entry.value->copy());
        return copy;
    }

    PAL::SessionID sessionID() const final { return m_sessionID; }
    void setSessionIDForTesting(PAL::SessionID sessionID) final { m_sessionID = sessionID; }
    uint64_t storageAreaMapCountForTesting() const final { return m_areas.size(); }

    StorageType m_type;
    unsigned m_quota;
    PAL::SessionID m_sessionID;
    String m_directory;
    RefPtr<const SecurityOrigin> m_topLevelOrigin;
    HashMap<SecurityOriginData, Ref<MlxStorageArea>> m_areas;
};

struct MlxStorageNamespaceProvider::State {
    // Each page's session storage, a namespace a top-level origin.
    HashMap<const Page*, HashMap<SecurityOriginData, Ref<StorageNamespace>>> sessions;
};

Ref<MlxStorageNamespaceProvider> MlxStorageNamespaceProvider::create(const String& directory)
{
    return adoptRef(*new MlxStorageNamespaceProvider(directory));
}

MlxStorageNamespaceProvider::MlxStorageNamespaceProvider(const String& directory)
    : m_directory(directory)
    , m_state(std::make_unique<State>())
{
}

MlxStorageNamespaceProvider::~MlxStorageNamespaceProvider() = default;

void MlxStorageNamespaceProvider::flush()
{
    syncAll();
}

void MlxStorageNamespaceProvider::pageClosed(Page& page)
{
    m_state->sessions.remove(&page);
}

RefPtr<StorageNamespace> MlxStorageNamespaceProvider::sessionStorageNamespace(const SecurityOrigin& topLevelOrigin, Page& page, ShouldCreateNamespace shouldCreate)
{
    auto pageEntry = m_state->sessions.find(&page);
    if (pageEntry == m_state->sessions.end()) {
        if (shouldCreate == ShouldCreateNamespace::No)
            return nullptr;
        pageEntry = m_state->sessions.add(&page, HashMap<SecurityOriginData, Ref<StorageNamespace>> { }).iterator;
    }
    auto& namespaces = pageEntry->value;
    auto entry = namespaces.find(topLevelOrigin.data());
    if (entry == namespaces.end()) {
        if (shouldCreate == ShouldCreateNamespace::No)
            return nullptr;
        Ref storageNamespace = MlxStorageNamespace::create(StorageType::Session, sessionStorageQuota(), page.sessionID(), String(), RefPtr { &topLevelOrigin });
        entry = namespaces.add(topLevelOrigin.data(), WTF::move(storageNamespace)).iterator;
    }
    return entry->value.ptr();
}

// A page opened by another (window.open) starts with a copy of its
// session storage.
void MlxStorageNamespaceProvider::cloneSessionStorageNamespaceForPage(Page& source, Page& destination)
{
    auto entry = m_state->sessions.find(&source);
    if (entry == m_state->sessions.end())
        return;
    HashMap<SecurityOriginData, Ref<StorageNamespace>> copies;
    for (auto& storageNamespace : entry->value)
        copies.add(storageNamespace.key, storageNamespace.value->copy(destination));
    m_state->sessions.set(&destination, WTF::move(copies));
}

Ref<StorageNamespace> MlxStorageNamespaceProvider::createLocalStorageNamespace(unsigned quota, PAL::SessionID sessionID)
{
    // (An ephemeral session's local storage is in memory.)
    String directory = sessionID.isEphemeral() ? String() : m_directory;
    if (storageTracing())
        fprintf(stderr, "storage: local storage in %s\n", directory.isNull() ? "memory" : directory.utf8().data());
    return MlxStorageNamespace::create(StorageType::Local, quota, sessionID, directory, nullptr);
}

Ref<StorageNamespace> MlxStorageNamespaceProvider::createTransientLocalStorageNamespace(SecurityOrigin& topLevelOrigin, unsigned quota, PAL::SessionID sessionID)
{
    return MlxStorageNamespace::create(StorageType::TransientLocal, quota, sessionID, String(), RefPtr<const SecurityOrigin> { &topLevelOrigin });
}

} // namespace WebCore
