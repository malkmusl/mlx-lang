// The Mlx port's NetworkStateNotifier platform functions (navigator.onLine
// and the online and offline events), which GLib's GNetworkMonitor gives
// the WPE port.
//
// STUB (M6: mlx-netd's link state, over MLXIPC, through the Mlx side): the
// network is taken to be up, and no change is observed.
#include "config.h"
#include "NetworkStateNotifier.h"

namespace WebCore {

void NetworkStateNotifier::updateStateWithoutNotifying()
{
    m_isOnLine = true;
}

void NetworkStateNotifier::startObserving()
{
}

} // namespace WebCore
