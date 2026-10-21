// Argument-dependent lookup joining function templates of two
// namespaces: WebCore's `add(hasher, a, b, c)` has WebCore's own add
// templates (add(Hasher &, const ProcessQualified<T> &)) in sight and finds
// WTF's variadic add template through the Hasher argument; the instance is
// chosen among both.
#include <cstdio>

namespace WTF {
struct Hasher { unsigned value = 0; };
struct Impl { int x; };
template<typename T> inline void add(Hasher& hasher, T* ptr) { hasher.value = hasher.value * 31 + (ptr ? 1 : 2); }
inline void add(Hasher& hasher, unsigned number) { hasher.value = hasher.value * 31 + number; }
template<typename T1, typename T2, typename... Others> void add(Hasher& hasher, const T1& value1, const T2& value2, const Others&... others)
{
    add(hasher, value1);
    add(hasher, value2, others...);
}
}
using WTF::Hasher;

namespace WebCore {
class Color {
public:
    Color(int r) : value(r) { }
    int value;
};
template<typename T> class Qualified {
public:
    Qualified(const T& object) : m_object(object) { }
    T m_object;
};
template<typename T> inline void add(Hasher& hasher, const Qualified<T>& q) { add(hasher, q.m_object); }
inline void add(Hasher& hasher, const Color& color) { add(hasher, static_cast<unsigned>(color.value)); }

inline void hashThree(Hasher& hasher, WTF::Impl* a, WTF::Impl* b, WTF::Impl* c)
{
    add(hasher, a, b, c);
}
}

int main()
{
    WTF::Impl i { 1 };
    Hasher h;
    WebCore::hashThree(h, &i, nullptr, &i);
    std::printf("%u\n", h.value);
    WebCore::add(h, WebCore::Color(3));
    WebCore::add(h, WebCore::Qualified<unsigned>(5u));
    add(h, &i, 7u);
    std::printf("%u\n", h.value);
    return static_cast<int>(h.value % 100);
}
