#include "shared.h"
namespace incomplete {
struct Hidden { unsigned a; unsigned b; };
struct State { unsigned long long big; unsigned small; };
unsigned weigh(const Hidden &hidden) { return hidden.a * 5 + hidden.b; }
unsigned stateOf(const State &state) { return (unsigned)(state.big % 1000) + state.small; }
Hidden *makeHidden(unsigned seed) { Hidden *h = new Hidden; h->a = seed; h->b = seed * seed; return h; }
State *makeState(unsigned seed) { State *s = new State; s->big = seed * 123457ull; s->small = seed; return s; }
void dropHidden(Hidden *hidden) { delete hidden; }
void dropState(State *state) { delete state; }
}
