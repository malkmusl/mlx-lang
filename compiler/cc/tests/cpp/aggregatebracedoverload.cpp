// Overloads given nested braces: an aggregate whose first member the
// first inner list does not make is no candidate (StaticRange::create({
// { node, offset }, { node, offset } }) takes a SimpleRange, not the
// Init whose first member is a Ref<Node>).
#include <cstdio>
#include <utility>
struct Node { int v; };
template<typename T> struct Ref {
    T* p;
    Ref(T& t) : p(&t) {}
    Ref(Ref&& o) : p(o.p) {}
    template<typename U> Ref(Ref<U>&& o) : p(o.p) {}
};
struct BoundaryPoint { Ref<Node> container; unsigned offset { 0 }; BoundaryPoint(Ref<Node>&& c, unsigned o) : container(std::move(c)), offset(o) {} };
struct SimpleRange { BoundaryPoint start, end; SimpleRange(BoundaryPoint&& a, BoundaryPoint&& b) : start(std::move(a)), end(std::move(b)) {} };
struct Init { Ref<Node> s; unsigned so { 0 }; Ref<Node> e; unsigned eo { 0 }; };
int create(Init&&) { return 1; }
int create(const SimpleRange&) { return 2; }
int create(SimpleRange&& r) { return 3 + r.end.offset + r.start.container.p->v; }
int main() {
    Node a{1}, b{2};
    Init init { a, 4, b, 5 };
    printf("%d\n", create({ { std::move(init.s), init.so }, { std::move(init.e), init.eo } }));
}
