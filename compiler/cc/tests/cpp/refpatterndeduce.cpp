// A reference type against a reference pattern: `Len &&` for `T &&`
// and `const Len &` for `const T &` deduce T as Len, what both refer
// to (an lvalue reference for `T &&` stays whole, the forwarding
// reference's T). WebCore's StyleTypeWrapper guide takes a getter
// returning `const T &` and a setter taking `T &&`.
#include <cstdio>
#include <type_traits>
struct Len { int v; };
struct Style {
    const Len& len() const { return m_len; }
    void setLen(Len&& l) { m_len = l; }
    Len m_len { 3 };
};
template<class T, class S> int both(const T& (S::*)() const, void (S::*)(T&&)) { return (int)sizeof(T); }
template<class T> struct Strip { using type = T; static constexpr int which = 0; };
template<class T> struct Strip<T&&> { using type = T; static constexpr int which = 2; };
template<class T> struct Strip<T&> { using type = T; static constexpr int which = 1; };
template<class T> int forwarding(T&& x) { return std::is_lvalue_reference_v<T> ? 10 : 20; }
int main()
{
    Len l;
    printf("%d %d %d %d %d %d\n", both(&Style::len, &Style::setLen), Strip<Len&&>::which, Strip<Len&>::which, Strip<Len>::which, forwarding(l), forwarding(Len { 1 }));
    printf("%d %d\n", (int)std::is_same_v<Strip<Len&&>::type, Len>, (int)std::is_same_v<Strip<const Len&>::type, const Len>);
    return 2;
}
