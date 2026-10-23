// The profile of the Mlx port's embedding (MlxProfile.h).
#include "config.h"
#include "MlxProfile.h"

#include "CookieJar.h"
#include "MlxIndexedDB.h"
#include "MlxNetwork.h"
#include "MlxStorage.h"
#include "MlxWebSocket.h"
#include "PageConfiguration.h"
#include "SocketProvider.h"
#include "port/MlxWebPort.h"
#include <wtf/FileSystem.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/text/CString.h>

namespace WebCore {

namespace MlxProfile {

struct Profile {
    String data;
    String cache;
    RefPtr<MlxStorageNamespaceProvider> storage;
    RefPtr<MlxDatabaseProvider> databases;
};

static Profile& profile()
{
    static NeverDestroyed<Profile> profile;
    return profile;
}

static String under(const String& directory, ASCIILiteral name)
{
    return directory.isEmpty() ? String() : FileSystem::pathByAppendingComponent(directory, String(name));
}

void open(const String& dataDirectory, const String& cacheDirectory)
{
    auto& p = profile();
    p.data = dataDirectory;
    p.cache = cacheDirectory;
    if (!p.data.isEmpty())
        FileSystem::makeAllDirectories(p.data);
    if (!p.cache.isEmpty())
        FileSystem::makeAllDirectories(p.cache);
    p.storage = MlxStorageNamespaceProvider::create(under(p.data, "LocalStorage"_s));
    p.databases = MlxDatabaseProvider::create(under(p.data, "IndexedDB"_s));
    // The network's: the cookies and the HTTP cache (on the network queue,
    // which every call of the Mlx side's network functions runs on).
    auto cookies = under(p.data, "cookies.sqlite"_s).utf8();
    auto http = under(p.cache, "http"_s).utf8();
    MlxNetwork::queue().dispatchSync([&] {
        mlxweb_profile_open(cookies.data(), http.data());
    });
}

const String& dataDirectory()
{
    return profile().data;
}

const String& cacheDirectory()
{
    return profile().cache;
}

void configure(PageConfiguration& configuration)
{
    auto& p = profile();
    if (!p.storage)
        open(String(), String());
    configuration.cookieJar = MlxNetwork::cookieJar();
    configuration.socketProvider = MlxWebSocket::socketProvider();
    configuration.storageNamespaceProvider = p.storage;
    configuration.databaseProvider = p.databases;
}

void pageOpened(Page& opener, Page& page)
{
    if (auto& storage = profile().storage)
        storage->cloneSessionStorageNamespaceForPage(opener, page);
}

void pageClosed(Page& page)
{
    if (auto& storage = profile().storage)
        storage->pageClosed(page);
}

void close()
{
    auto& p = profile();
    if (p.storage)
        p.storage->flush();
    if (p.databases)
        p.databases->flush();
    MlxNetwork::queue().dispatchSync([] {
        mlxweb_profile_close();
    });
}

} // namespace MlxProfile

} // namespace WebCore
