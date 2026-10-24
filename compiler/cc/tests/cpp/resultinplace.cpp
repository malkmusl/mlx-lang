// A call's class result made at the object a member initializer (or any
// construction) makes, not moved there (C++17): WebCore's
// RelayoutScopeForScrollbarChange initializes `std::optional<
// ScrollbarUpdateScope>` from a call, and the move would copy the scope's
// const CheckedRef of a class only declared there (rendering-7). In
// constant evaluation too.
#include <cstdio>
#include <optional>
struct Area;
struct Ref {
    Area* p;
    explicit Ref(Area& a) : p(&a) { }
    Ref(const Ref& other);
};
struct Scope {
    explicit Scope(Area& a) : ref(a), value(7) { }
    Scope(Scope&&) = default;
    Scope(const Scope&) = delete;
    const Ref ref;
    int value;
};
struct Block {
    std::optional<Scope> update();
};
struct Relayout {
    Relayout(Block& b) : scope(b.update()) { }
    std::optional<Scope> scope;
};
struct Area { int count = 0; void increment() { count++; } };
Ref::Ref(const Ref& other) : p(other.p) { p->increment(); }
Area area;
std::optional<Scope> Block::update() { return Scope(area); }
struct Pair { int a; int b; constexpr Pair(int x) : a(x), b(x * 2) { } constexpr ~Pair() { } };
constexpr Pair makePair(int x) { return Pair(x); }
struct Holder { Pair pair; constexpr Holder(int x) : pair(makePair(x)) { } };
constexpr Holder holder(5);
static_assert(holder.pair.b == 10);
int main() {
    Block b;
    Relayout r(b);
    std::printf("%d %d %d\n", r.scope->value, area.count, Holder(3).pair.b);
    return 0;
}
