// IndexedDB of the Mlx port's embedding (MlxIndexedDB.h).
#include "config.h"
#include "MlxIndexedDB.h"

#include "IDBConnectionToClient.h"
#include "IDBConnectionToClientDelegate.h"
#include "IDBConnectionToServer.h"
#include "IDBConnectionToServerDelegate.h"
#include "IDBCursorInfo.h"
#include "IDBDatabaseNameAndVersion.h"
#include "IDBError.h"
#include "IDBGetAllRecordsData.h"
#include "IDBGetRecordData.h"
#include "IDBIndexInfo.h"
#include "IDBIterateCursorData.h"
#include "IDBKeyData.h"
#include "IDBKeyRangeData.h"
#include "IDBObjectStoreInfo.h"
#include "IDBOpenRequestData.h"
#include "IDBRequestData.h"
#include "IDBResultData.h"
#include "IDBServer.h"
#include "IDBTransactionInfo.h"
#include "IDBValue.h"
#include "IndexKey.h"
#include "ProcessIdentifier.h"
#include "UniqueIDBDatabaseConnection.h"
#include <pal/SessionID.h>
#include <stdio.h>
#include <stdlib.h>
#include <wtf/CrossThreadCopier.h>
#include <wtf/FileSystem.h>
#include <wtf/RunLoop.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/WorkQueue.h>
#include <wtf/text/CString.h>

namespace WebCore {

// MLX_IDB_TRACE in the environment: the server's start, on standard error.
static bool idbTracing()
{
    static bool trace = getenv("MLX_IDB_TRACE");
    return trace;
}

// The server's half: the IDBConnectionToClient's delegate, on the
// server's queue, which sends each answer to the client's half.
class MlxIDBClientEnd;

// The client's half (the main thread's): the IDBConnectionToServer's
// delegate, which sends each call to the server's queue.
class MlxIDBServerConnection final : public IDBClient::IDBConnectionToServerDelegate, public ThreadSafeRefCounted<MlxIDBServerConnection> {
public:
    static Ref<MlxIDBServerConnection> create(const String& directory)
    {
        return adoptRef(*new MlxIDBServerConnection(directory));
    }

    void ref() const final { ThreadSafeRefCounted::ref(); }
    void deref() const final { ThreadSafeRefCounted::deref(); }

    IDBClient::IDBConnectionToServer& connectionToServer() { return *m_connectionToServer; }

    // On the server's queue.
    void dispatchTask(Function<void(IDBServer::IDBServer&)>&& task)
    {
        m_queue->dispatch([this, protectedThis = Ref { *this }, task = WTF::move(task)] {
            task(*m_server);
        });
    }

    // On the main thread.
    void dispatchReply(Function<void(IDBClient::IDBConnectionToServer&)>&& task)
    {
        RunLoop::mainSingleton().dispatch([this, protectedThis = Ref { *this }, task = WTF::move(task)] {
            task(*m_connectionToServer);
        });
    }

    void flush()
    {
        m_queue->dispatchSync([] { });
    }

private:
    explicit MlxIDBServerConnection(const String& directory);

    std::optional<IDBConnectionIdentifier> identifier() const final { return Process::identifier(); }

    void deleteDatabase(const IDBOpenRequestData& data) final
    {
        dispatchTask([data = crossThreadCopy(data)](auto& server) { server.deleteDatabase(data); });
    }
    void openDatabase(const IDBOpenRequestData& data) final
    {
        dispatchTask([data = crossThreadCopy(data)](auto& server) { server.openDatabase(data); });
    }
    void abortTransaction(const IDBResourceIdentifier& identifier) final
    {
        dispatchTask([identifier](auto& server) { server.abortTransaction(identifier); });
    }
    void commitTransaction(const IDBResourceIdentifier& identifier, uint64_t handledRequestResultsCount) final
    {
        dispatchTask([identifier, handledRequestResultsCount](auto& server) { server.commitTransaction(identifier, handledRequestResultsCount); });
    }
    void didFinishHandlingVersionChangeTransaction(IDBDatabaseConnectionIdentifier connection, const IDBResourceIdentifier& identifier) final
    {
        dispatchTask([connection, identifier](auto& server) { server.didFinishHandlingVersionChangeTransaction(connection, identifier); });
    }
    void createObjectStore(const IDBRequestData& request, const IDBObjectStoreInfo& info) final
    {
        dispatchTask([request = crossThreadCopy(request), info = crossThreadCopy(info)](auto& server) { server.createObjectStore(request, info); });
    }
    void deleteObjectStore(const IDBRequestData& request, const String& name) final
    {
        dispatchTask([request = crossThreadCopy(request), name = name.isolatedCopy()](auto& server) { server.deleteObjectStore(request, name); });
    }
    void renameObjectStore(const IDBRequestData& request, IDBObjectStoreIdentifier store, const String& name) final
    {
        dispatchTask([request = crossThreadCopy(request), store, name = name.isolatedCopy()](auto& server) { server.renameObjectStore(request, store, name); });
    }
    void clearObjectStore(const IDBRequestData& request, IDBObjectStoreIdentifier store) final
    {
        dispatchTask([request = crossThreadCopy(request), store](auto& server) { server.clearObjectStore(request, store); });
    }
    void createIndex(const IDBRequestData& request, const IDBIndexInfo& info) final
    {
        dispatchTask([request = crossThreadCopy(request), info = crossThreadCopy(info)](auto& server) { server.createIndex(request, info); });
    }
    void deleteIndex(const IDBRequestData& request, IDBObjectStoreIdentifier store, const String& name) final
    {
        dispatchTask([request = crossThreadCopy(request), store, name = name.isolatedCopy()](auto& server) { server.deleteIndex(request, store, name); });
    }
    void renameIndex(const IDBRequestData& request, IDBObjectStoreIdentifier store, IDBIndexIdentifier index, const String& name) final
    {
        dispatchTask([request = crossThreadCopy(request), store, index, name = name.isolatedCopy()](auto& server) { server.renameIndex(request, store, index, name); });
    }
    void putOrAdd(const IDBRequestData& request, const IDBKeyData& key, const IDBValue& value, const IndexIDToIndexKeyMap& indexKeys, const IndexedDB::ObjectStoreOverwriteMode mode) final
    {
        dispatchTask([request = crossThreadCopy(request), key = crossThreadCopy(key), value = crossThreadCopy(value), indexKeys = crossThreadCopy(indexKeys), mode](auto& server) { server.putOrAdd(request, key, value, indexKeys, mode); });
    }
    void getRecord(const IDBRequestData& request, const IDBGetRecordData& data) final
    {
        dispatchTask([request = crossThreadCopy(request), data = crossThreadCopy(data)](auto& server) { server.getRecord(request, data); });
    }
    void getAllRecords(const IDBRequestData& request, const IDBGetAllRecordsData& data) final
    {
        dispatchTask([request = crossThreadCopy(request), data = crossThreadCopy(data)](auto& server) { server.getAllRecords(request, data); });
    }
    void getCount(const IDBRequestData& request, const IDBKeyRangeData& range) final
    {
        dispatchTask([request = crossThreadCopy(request), range = crossThreadCopy(range)](auto& server) { server.getCount(request, range); });
    }
    void deleteRecord(const IDBRequestData& request, const IDBKeyRangeData& range) final
    {
        dispatchTask([request = crossThreadCopy(request), range = crossThreadCopy(range)](auto& server) { server.deleteRecord(request, range); });
    }
    void openCursor(const IDBRequestData& request, const IDBCursorInfo& info) final
    {
        dispatchTask([request = crossThreadCopy(request), info = crossThreadCopy(info)](auto& server) { server.openCursor(request, info); });
    }
    void iterateCursor(const IDBRequestData& request, const IDBIterateCursorData& data) final
    {
        dispatchTask([request = crossThreadCopy(request), data = crossThreadCopy(data)](auto& server) { server.iterateCursor(request, data); });
    }
    void establishTransaction(IDBDatabaseConnectionIdentifier connection, const IDBTransactionInfo& info) final
    {
        dispatchTask([connection, info = crossThreadCopy(info)](auto& server) { server.establishTransaction(connection, info); });
    }
    void databaseConnectionPendingClose(IDBDatabaseConnectionIdentifier connection) final
    {
        dispatchTask([connection](auto& server) { server.databaseConnectionPendingClose(connection); });
    }
    void databaseConnectionClosed(IDBDatabaseConnectionIdentifier connection) final
    {
        dispatchTask([connection](auto& server) { server.databaseConnectionClosed(connection); });
    }
    void abortOpenAndUpgradeNeeded(IDBDatabaseConnectionIdentifier connection, const std::optional<IDBResourceIdentifier>& transaction) final
    {
        dispatchTask([connection, transaction](auto& server) { server.abortOpenAndUpgradeNeeded(connection, transaction); });
    }
    void didFireVersionChangeEvent(IDBDatabaseConnectionIdentifier connection, const IDBResourceIdentifier& request, const IndexedDB::ConnectionClosedOnBehalfOfServer closed) final
    {
        dispatchTask([connection, request, closed](auto& server) { server.didFireVersionChangeEvent(connection, request, closed); });
    }
    void didGenerateIndexKeyForRecord(const IDBResourceIdentifier& transaction, const IDBResourceIdentifier& request, const IDBIndexInfo& info, const IDBKeyData& key, const IndexKey& indexKey, std::optional<int64_t> recordID) final
    {
        dispatchTask([transaction, request, info = crossThreadCopy(info), key = crossThreadCopy(key), indexKey = crossThreadCopy(indexKey), recordID](auto& server) { server.didGenerateIndexKeyForRecord(transaction, request, info, key, indexKey, recordID); });
    }
    void openDBRequestCancelled(const IDBOpenRequestData& data) final
    {
        dispatchTask([data = crossThreadCopy(data)](auto& server) { server.openDBRequestCancelled(data); });
    }
    void getAllDatabaseNamesAndVersions(const IDBResourceIdentifier& request, const ClientOrigin& origin) final
    {
        dispatchTask([request, origin = crossThreadCopy(origin)](auto& server) { server.getAllDatabaseNamesAndVersions(Process::identifier(), request, origin); });
    }

    Ref<WorkQueue> m_queue;
    RefPtr<IDBClient::IDBConnectionToServer> m_connectionToServer;
    // The server's, made and used on its queue only.
    std::unique_ptr<MlxIDBClientEnd> m_clientEnd;
    RefPtr<IDBServer::IDBConnectionToClient> m_connectionToClient;
    std::unique_ptr<IDBServer::IDBServer> m_server;
};

class MlxIDBClientEnd final : public IDBServer::IDBConnectionToClientDelegate {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED(MlxIDBClientEnd);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(MlxIDBClientEnd);
public:
    explicit MlxIDBClientEnd(MlxIDBServerConnection& connection)
        : m_connection(connection)
    {
    }

private:
    void reply(Function<void(IDBClient::IDBConnectionToServer&)>&& task) { m_connection.dispatchReply(WTF::move(task)); }

    std::optional<IDBConnectionIdentifier> identifier() const final { return Process::identifier(); }

    void didDeleteDatabase(const IDBResultData& result) final
    {
        reply([result = crossThreadCopy(result)](auto& client) { client.didDeleteDatabase(result); });
    }
    void didOpenDatabase(const IDBResultData& result) final
    {
        reply([result = crossThreadCopy(result)](auto& client) { client.didOpenDatabase(result); });
    }
    void didAbortTransaction(const IDBResourceIdentifier& transaction, const IDBError& error) final
    {
        reply([transaction, error = crossThreadCopy(error)](auto& client) { client.didAbortTransaction(transaction, error); });
    }
    void didCommitTransaction(const IDBResourceIdentifier& transaction, const IDBError& error) final
    {
        reply([transaction, error = crossThreadCopy(error)](auto& client) { client.didCommitTransaction(transaction, error); });
    }
    void didCreateObjectStore(const IDBResultData& result) final
    {
        reply([result = crossThreadCopy(result)](auto& client) { client.didCreateObjectStore(result); });
    }
    void didDeleteObjectStore(const IDBResultData& result) final
    {
        reply([result = crossThreadCopy(result)](auto& client) { client.didDeleteObjectStore(result); });
    }
    void didRenameObjectStore(const IDBResultData& result) final
    {
        reply([result = crossThreadCopy(result)](auto& client) { client.didRenameObjectStore(result); });
    }
    void didClearObjectStore(const IDBResultData& result) final
    {
        reply([result = crossThreadCopy(result)](auto& client) { client.didClearObjectStore(result); });
    }
    void didCreateIndex(const IDBResultData& result) final
    {
        reply([result = crossThreadCopy(result)](auto& client) { client.didCreateIndex(result); });
    }
    void didDeleteIndex(const IDBResultData& result) final
    {
        reply([result = crossThreadCopy(result)](auto& client) { client.didDeleteIndex(result); });
    }
    void didRenameIndex(const IDBResultData& result) final
    {
        reply([result = crossThreadCopy(result)](auto& client) { client.didRenameIndex(result); });
    }
    void didPutOrAdd(const IDBResultData& result) final
    {
        reply([result = crossThreadCopy(result)](auto& client) { client.didPutOrAdd(result); });
    }
    void didGetRecord(const IDBResultData& result) final
    {
        reply([result = crossThreadCopy(result)](auto& client) { client.didGetRecord(result); });
    }
    void didGetAllRecords(const IDBResultData& result) final
    {
        reply([result = crossThreadCopy(result)](auto& client) { client.didGetAllRecords(result); });
    }
    void didGetCount(const IDBResultData& result) final
    {
        reply([result = crossThreadCopy(result)](auto& client) { client.didGetCount(result); });
    }
    void didDeleteRecord(const IDBResultData& result) final
    {
        reply([result = crossThreadCopy(result)](auto& client) { client.didDeleteRecord(result); });
    }
    void didOpenCursor(const IDBResultData& result) final
    {
        reply([result = crossThreadCopy(result)](auto& client) { client.didOpenCursor(result); });
    }
    void didIterateCursor(const IDBResultData& result) final
    {
        reply([result = crossThreadCopy(result)](auto& client) { client.didIterateCursor(result); });
    }
    void fireVersionChangeEvent(IDBServer::UniqueIDBDatabaseConnection& connection, const IDBResourceIdentifier& request, uint64_t requestedVersion) final
    {
        reply([connection = connection.identifier(), request, requestedVersion](auto& client) { client.fireVersionChangeEvent(connection, request, requestedVersion); });
    }
    void generateIndexKeyForRecord(const IDBResourceIdentifier& request, const IDBIndexInfo& info, const std::optional<IDBKeyPath>& keyPath, const IDBKeyData& key, const IDBValue& value, std::optional<int64_t> recordID) final
    {
        reply([request, info = crossThreadCopy(info), keyPath = crossThreadCopy(keyPath), key = crossThreadCopy(key), value = crossThreadCopy(value), recordID](auto& client) { client.generateIndexKeyForRecord(request, info, keyPath, key, value, recordID); });
    }
    void didStartTransaction(const IDBResourceIdentifier& transaction, const IDBError& error) final
    {
        reply([transaction, error = crossThreadCopy(error)](auto& client) { client.didStartTransaction(transaction, error); });
    }
    void didCloseFromServer(IDBServer::UniqueIDBDatabaseConnection& connection, const IDBError& error) final
    {
        reply([connection = connection.identifier(), error = crossThreadCopy(error)](auto& client) { client.didCloseFromServer(connection, error); });
    }
    void notifyOpenDBRequestBlocked(const IDBResourceIdentifier& request, uint64_t oldVersion, uint64_t newVersion) final
    {
        reply([request, oldVersion, newVersion](auto& client) { client.notifyOpenDBRequestBlocked(request, oldVersion, newVersion); });
    }
    void didGetAllDatabaseNamesAndVersions(const IDBResourceIdentifier& request, Vector<IDBDatabaseNameAndVersion>&& databases) final
    {
        reply([request, databases = crossThreadCopy(WTF::move(databases))](auto& client) mutable { client.didGetAllDatabaseNamesAndVersions(request, WTF::move(databases)); });
    }

    MlxIDBServerConnection& m_connection;
};

MlxIDBServerConnection::MlxIDBServerConnection(const String& directory)
    : m_queue(WorkQueue::create("org.mlx.indexeddb"_s))
{
    m_connectionToServer = IDBClient::IDBConnectionToServer::create(*this, PAL::SessionID::defaultSessionID());
    if (idbTracing())
        fprintf(stderr, "indexeddb: the server keeps its databases in %s\n", directory.isNull() ? "memory" : directory.utf8().data());
    m_queue->dispatch([this, protectedThis = Ref { *this }, directory = directory.isolatedCopy()] {
        if (!directory.isNull())
            FileSystem::makeAllDirectories(directory);
        m_clientEnd = makeUnique<MlxIDBClientEnd>(*this);
        m_connectionToClient = IDBServer::IDBConnectionToClient::create(*m_clientEnd);
        // (Space is not counted against a quota yet: every request has it.)
        m_server = makeUnique<IDBServer::IDBServer>(directory, [](const ClientOrigin&, uint64_t) {
            return true;
        });
        m_server->registerConnection(*m_connectionToClient);
    });
}

Ref<MlxDatabaseProvider> MlxDatabaseProvider::create(const String& directory)
{
    return adoptRef(*new MlxDatabaseProvider(directory));
}

MlxDatabaseProvider::MlxDatabaseProvider(const String& directory)
    : m_directory(directory)
{
}

MlxDatabaseProvider::~MlxDatabaseProvider() = default;

IDBClient::IDBConnectionToServer& MlxDatabaseProvider::idbConnectionToServerForSession(PAL::SessionID)
{
    if (!m_connection)
        m_connection = MlxIDBServerConnection::create(m_directory);
    return m_connection->connectionToServer();
}

void MlxDatabaseProvider::flush()
{
    if (m_connection)
        m_connection->flush();
}

} // namespace WebCore
