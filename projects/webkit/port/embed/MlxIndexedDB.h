// IndexedDB of the Mlx port's embedding (PORT.md, M7): WebKit's IDB server
// (WebCore's IDBServer::IDBServer, UniqueIDBDatabase and its SQLite backing
// store, SQLiteIDBBackingStore over WebCore's SQLiteDatabase, which runs on
// mlx-sqlite3) in the process, on a thread of its own, and the pages'
// connection to it, as WebKitLegacy's InProcessIDBServer had them: each
// call of the client's IDBConnectionToServer goes to the server's queue
// with its data copied for that thread, each answer of the server back to
// the main thread the same way (WebKit2 sends both through IPC between the
// web and the network process).
//
// The databases are kept in DIRECTORY (IDBServer's layout:
// v1/<top origin>/<origin>/<database's hash>/IndexedDB.sqlite3); with no
// directory, IDBServer keeps them in memory (MemoryIDBBackingStore). Every
// page of the process shares the one server (the page configuration's
// DatabaseProvider).
#pragma once

#include "DatabaseProvider.h"
#include <wtf/Forward.h>

namespace WebCore {

class MlxIDBServerConnection;

class MlxDatabaseProvider final : public DatabaseProvider {
public:
    // DIRECTORY: where the databases are kept (null: in memory).
    WEBCORE_EXPORT static Ref<MlxDatabaseProvider> create(const String& directory);
    WEBCORE_EXPORT ~MlxDatabaseProvider();

    // Waits until the server has done what it was asked (before exit).
    WEBCORE_EXPORT void flush();

private:
    explicit MlxDatabaseProvider(const String& directory);
    IDBClient::IDBConnectionToServer& idbConnectionToServerForSession(PAL::SessionID) final;

    String m_directory;
    RefPtr<MlxIDBServerConnection> m_connection;
};

} // namespace WebCore
