// A base's member type named in a member function (`Error::NoError`)
// before a name a using-directive brings in: WebCore's
// BlobResourceHandle reads its base's enum Error where
// CacheStorageConnection.cpp says `using namespace DOMCacheEngine;`
// (whose Error has no NoError), before its class and after it (a body
// read late, once its function is known virtual).
#include <cstdio>
namespace W {
namespace Early {
enum class Error { Other };
}
using namespace W::Early;
struct Client {
    virtual ~Client() { }
};
class HandleBase : public Client {
public:
    enum class Error { NoError = 0, NotFound = 1 };
    virtual bool erroredOrAborted() const = 0;
    virtual void didFail(Error) = 0;
};
class Handle {
public:
    virtual ~Handle() { }
    void ref() const { }
};
class BlobHandle final : public HandleBase, public Handle {
public:
    bool aborted() const { return m_aborted; }
    void didFail(Error e) final { m_errorCode = e; }
    bool erroredOrAborted() const final { return m_aborted || m_errorCode != Error::NoError; }
    int code() const { return static_cast<int>(m_errorCode) + static_cast<int>(Error::NotFound); }
    Error m_errorCode { Error::NoError };
    bool m_aborted { false };
};
namespace Late {
enum class Error { Another };
}
using namespace W::Late;
}

int main()
{
    W::BlobHandle h;
    W::HandleBase* b = &h;
    int before = b->erroredOrAborted();
    b->didFail(W::HandleBase::Error::NotFound);
    printf("%d %d %d\n", before, b->erroredOrAborted(), h.code());
    return 0;
}
