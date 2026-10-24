// std::span deduced from a contiguous range whose data() is private
// (WTF::Vector's): the element type from its begin(), as libc++'s guide
// takes it (`std::span(tokens).subspan(startIndex)` in WebCore's
// StyleSubstitutionResolver).
#include <cstdio>
#include <span>
template<typename T> struct Buf {
    T items[4] { 1, 2, 3, 4 };
    T* buffer() { return items; }
    const T* buffer() const { return items; }
};
template<typename T, size_t inlineCapacity = 0> class Vec : private Buf<T> {
    using Base = Buf<T>;
public:
    std::span<const T> span() const { return std::span<const T>(data(), size()); }
    T* begin() { return data(); }
    T* end() { return data() + 4; }
    const T* begin() const { return data(); }
    const T* end() const { return data() + 4; }
    size_t size() const { return 4; }
private:
    T* data() { return Base::buffer(); }
    const T* data() const { return Base::buffer(); }
};
struct Tok { int v; Tok(int v) : v(v) { } };
int sum(std::span<const Tok> s) { int r = 0; for (auto& t : s) r += t.v; return r; }
bool g(Vec<Tok>& tokens) {
    auto start = 1;
    auto f = [&] { return sum(std::span(tokens).subspan(start)); };
    return f() == 9;
}
int main() {
    Vec<Tok> v;
    printf("%d\n", sum(std::span(v).subspan(1)));
    printf("%d\n", g(v));
    const Vec<Tok>& c = v;
    auto s = std::span(c);
    printf("%d %d\n", (int)s.size(), s[2].v);
    int raw[3] = { 5, 6, 7 };
    auto r = std::span(raw);
    printf("%d %d\n", (int)r.size(), r[1]);
}
