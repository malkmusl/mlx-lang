#include "shared.h"
namespace incomplete {
static unsigned through(const Holder &holder)
{
    return weigh(holder.get()) * 3 + stateOf(holder.getState());
}
unsigned run()
{
    Hidden *hidden = makeHidden(7);
    State *state = makeState(11);
    Holder holder(*hidden, *state);
    unsigned total = through(holder);
    Hidden &again = holder.get();
    total += weigh(again);
    dropHidden(hidden);
    dropState(state);
    return total;
}
}
