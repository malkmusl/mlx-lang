// An empty braced argument against overloads: the candidate whose
// class parameter has no default constructor (Derived inherits
// Base(int) and no default one; its implicit one is deleted) is not
// viable, so the other is chosen, as g++ does (WebCore's
// MediaSession::callActionHandler of a DOMPromiseDeferred<void> or a
// TriggerGestureIndicator).
#include <cstdio>
enum class Trigger : bool { No, Yes };
struct Base {
    int v;
    explicit Base(int x) : v(x) { }
    Base(Base&&) = default;
};
struct Derived : Base {
    using Base::Base;
};
struct S {
    void call(const int& a, Derived&& d) { printf("derived %d %d\n", a, d.v); }
    void call(const int& a, Trigger t = Trigger::Yes) { printf("trigger %d %d\n", a, static_cast<int>(t)); }
};
static void free1(Derived&& d) { printf("free derived %d\n", d.v); }
static void free1(Trigger t) { printf("free trigger %d\n", static_cast<int>(t)); }
int main()
{
    S s;
    s.call(1, { });
    s.call(2, Derived(5));
    s.call(3);
    free1({ });
    free1(Derived(9));
    return 0;
}
