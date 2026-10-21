// Ref-qualified member functions: `&` for lvalue objects, `&&` for
// rvalue ones (a temporary, std::move, std::forward), `const &` for
// both; overloads of their own with their own symbols (R, O). WTF's
// Expected takes `std::forward<unexpected<E>>(u).error()` so, moving a
// unique_ptr out.
#include <cstdio>
#include <memory>
#include <utility>
struct U {
    int val = 1;
    const int& error() const& { printf("const&\n"); return val; }
    int& error() & { printf("&\n"); return val; }
    int&& error() && { printf("&&\n"); return std::move(val); }
};
U make() { return U(); }
template<class E> struct unexpected {
    E value;
    explicit unexpected(E&& e) : value(std::move(e)) {}
    const E& error() const& { return value; }
    E&& error() && { return std::move(value); }
};
struct Holder { std::unique_ptr<int> p; Holder(const std::unique_ptr<int>&) = delete; Holder(std::unique_ptr<int>&& q) : p(std::move(q)) {} };
struct Out { int get() &; int get() &&; };
int Out::get() & { return 1; }
int Out::get() && { return 2; }
int main() {
    U u; const U cu;
    u.error(); cu.error(); std::move(u).error(); make().error(); std::forward<U>(u).error();
    U* p = &u; p->error();
    unexpected<std::unique_ptr<int>> e(std::make_unique<int>(3));
    Holder h(std::forward<unexpected<std::unique_ptr<int>>>(e).error());
    Out o;
    printf("%d %d %d\n", *h.p, o.get(), Out().get());
    return 0;
}
