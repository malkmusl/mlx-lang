// The Mlx port's DNS queue (DNS prefetching, and resolveDNS of
// DNS.h): DNSResolveQueue.cpp makes its singleton of the type a port names
// DNSResolveQueuePlatform (GLib's, curl's, CFNetwork's), so it is compiled
// here, after the port's class, instead of in its unified source
// (lists/webcore-mlx-skipped.txt).
//
// STUB (M6: names resolved by std.dns through the Mlx side): nothing is
// resolved yet; a request fails as CannotResolve and nothing is
// prefetched (the queue takes the network to be behind a proxy, which
// DNSResolveQueue does not prefetch for).
#include "config.h"
#include "DNSResolveQueue.h"

#include "DNS.h"
#include <wtf/CompletionHandler.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

class DNSResolveQueueMlx final : public DNSResolveQueue {
    WTF_MAKE_TZONE_ALLOCATED(DNSResolveQueueMlx);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(DNSResolveQueueMlx);
public:
    DNSResolveQueueMlx() = default;

    void resolve(const String&, uint64_t, DNSCompletionHandler&& completionHandler) final
    {
        completionHandler(makeUnexpected(DNSError::CannotResolve));
    }

    void stopResolve(uint64_t) final { }

private:
    void updateIsUsingProxy() final { m_isUsingProxy = true; }
    void platformResolve(const String&) final { decrementRequestCount(); }
};

WTF_MAKE_TZONE_ALLOCATED_IMPL(DNSResolveQueueMlx);

using DNSResolveQueuePlatform = DNSResolveQueueMlx;

} // namespace WebCore

#include "platform/network/DNSResolveQueue.cpp"
