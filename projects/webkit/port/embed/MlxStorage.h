// Web storage of the Mlx port's embedding (PORT.md, M7): localStorage and
// sessionStorage, the StorageNamespaceProvider every page of the process
// shares (PageConfiguration::storageNamespaceProvider), as WebKit2's
// WebStorageNamespaceProvider and the network process's storage managers
// are together, in one process.
//
// localStorage: one area an origin, shared by every page and frame of the
// origin, read from its database the first time a script uses it and
// written back to it a second after a change (and when the profile is
// closed): DIRECTORY/<the origin's database identifier>.localstorage,
// an SQLite database (WebCore's SQLiteDatabase, on mlx-sqlite3) with
// WebKit's table, `ItemTable (key TEXT UNIQUE ON CONFLICT REPLACE, value
// BLOB NOT NULL ON CONFLICT FAIL)`, the values as UTF-16 (WebKit2's
// SQLiteStorageArea). With no directory, and for an ephemeral session or a
// third party's partitioned storage, the areas live in memory only.
// sessionStorage: a namespace a page (a tab) and top-level origin, in
// memory. The quotas are WebKit's: 5 MB an origin for local storage
// (StorageNamespaceProvider's localStorageDatabaseQuotaInBytes), the
// page's SessionStorageQuota setting (5 MB) for session storage.
//
// A change fires `storage` events at the other windows of the origin
// (StorageEventDispatcher): of every page for local storage (other tabs
// too), of the same page for session storage; not at the window that made
// the change.
#pragma once

#include "StorageNamespaceProvider.h"
#include <wtf/Forward.h>

namespace WebCore {

class MlxStorageNamespaceProvider final : public StorageNamespaceProvider {
public:
    // DIRECTORY: where local storage is kept (made when missing); null: in
    // memory only.
    WEBCORE_EXPORT static Ref<MlxStorageNamespaceProvider> create(const String& directory);
    WEBCORE_EXPORT ~MlxStorageNamespaceProvider();

    // Writes local storage's pending changes to the databases now.
    WEBCORE_EXPORT void flush();
    // Gives a page opened by another a copy of its session storage.
    WEBCORE_EXPORT void cloneSessionStorageNamespaceForPage(Page& source, Page& destination) final;
    // Forgets the session storage of a page that is closed.
    WEBCORE_EXPORT void pageClosed(Page&);

private:
    explicit MlxStorageNamespaceProvider(const String& directory);

    RefPtr<StorageNamespace> sessionStorageNamespace(const SecurityOrigin&, Page&, ShouldCreateNamespace) final;
    Ref<StorageNamespace> createLocalStorageNamespace(unsigned quota, PAL::SessionID) final;
    Ref<StorageNamespace> createTransientLocalStorageNamespace(SecurityOrigin&, unsigned quota, PAL::SessionID) final;

    String m_directory;
    struct State;
    std::unique_ptr<State> m_state;
};

} // namespace WebCore
