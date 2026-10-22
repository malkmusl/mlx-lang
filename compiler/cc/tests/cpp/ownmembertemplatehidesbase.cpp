// A class's own member function template called by its name in a
// member function hides a base's member template of the name (JSC's
// JSRopeString::resolveToBuffer(JSString *, JSString *, JSString *, span,
// uint8_t *) beside JSString::resolveToBuffer(span)); a base's hides a
// namespace's only.
#include <cstdio>
#include <span>
struct Base {
    template<typename C> inline void fill(std::span<C>);
};
struct Rope final : Base {
    Base* a() const { return nullptr; }
    template<typename C> void resolve(std::span<C> buffer, int* limit) const;
    template<typename C> static void fill(Base*, Base*, std::span<C> buffer, int* limit);
};
template<typename C> void Rope::fill(Base*, Base*, std::span<C> buffer, int* limit)
{
    for (auto& c : buffer)
        c = (C)*limit;
}
template<typename C> void Rope::resolve(std::span<C> buffer, int* limit) const
{
    fill(a(), a(), buffer, limit);
}
int main()
{
    Rope r;
    char text[3] = { 0, 0, 0 };
    int limit = 'x';
    r.resolve(std::span<char>(text, 2), &limit);
    printf("%s\n", text);
    return 0;
}
