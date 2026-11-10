#pragma once
namespace incomplete {
struct Hidden;
struct State;
struct Holder
{
    Holder(Hidden &x, const State &s) : h(&x), state(s) {}
    Hidden &get() const { return *h; }
    const State &getState() const { return state; }
    Hidden *h;
    const State &state;
};
unsigned weigh(const Hidden &hidden);
unsigned stateOf(const State &state);
Hidden *makeHidden(unsigned seed);
State *makeState(unsigned seed);
void dropHidden(Hidden *hidden);
void dropState(State *state);
}
