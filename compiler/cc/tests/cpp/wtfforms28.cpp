// WTF/JSC forms 28: inherited constructors from braces, constrained partial specializations ranked below forms, CTAD in expressions
#include <stdio.h>
#include <vector>
#include <utility>
#include <span>

namespace p_inhctor {
template<typename T> class Stream {
public:
    using Buffer = std::vector<unsigned char>;
private:
    template<class B> class BaseRef {
        template<typename> friend class Stream;
    public:
        BaseRef(const BaseRef<B>& o) : m_buf(o.m_buf), m_index(o.m_index) { }
        unsigned offset() const { return m_index; }
    protected:
        BaseRef(B& b, size_t i) : m_buf(b), m_index((unsigned)i) { }
        B& m_buf;
        unsigned m_index;
    };
public:
    using Ref = BaseRef<const Buffer>;
private:
    class iterator : public Ref {
        template<typename> friend class Stream;
    public:
        using Ref::Ref;
        using Ref::m_index;
        Ref& operator*() { return *this; }
        iterator& operator++() { m_index += 2; return *this; }
        bool operator!=(const iterator& o) const { return m_index != o.m_index; }
    };
public:
    iterator begin() const { return iterator { m_buf, 0 }; }
    iterator end() const { return iterator { m_buf, m_buf.size() }; }
    Buffer m_buf { 1, 2, 3, 4, 5, 6 };
};
int run() { Stream<int> s; unsigned t = 0; for (auto& r : s) t += r.offset(); printf("%u\n", t); return 0; }
}

namespace p_inhctor6 {
template<class B> class BaseRef {
public:
    BaseRef(const BaseRef<B>& o) : m_buf(o.m_buf), m_index(o.m_index) { }
    BaseRef(B& b, unsigned long i) : m_buf(b), m_index((unsigned)i) { }
    unsigned offset() const { return m_index; }
    B& m_buf;
    unsigned m_index;
};
using Ref = BaseRef<const int>;
class iterator : public Ref { public: using Ref::Ref; };
struct S {
    int buf = 4;
    iterator a() const { return iterator { buf, 0 }; }
    iterator b() const { return iterator { buf, 6 }; }
};
int run() { S s; printf("%u %u\n", s.a().offset(), s.b().offset()); return 0; }
}

namespace p_partcon {
template<typename T> struct DH;
template<typename T> concept Hashable = requires(const T& t) { t.h(); };
template<typename T, typename U> struct DH<std::pair<T, U>> { static int kind() { return 2; } };
template<Hashable T> struct DH<T> { static int kind() { return 1; } };
template<> struct DH<int> { static int kind() { return 0; } };
struct P { int h() const { return 1; } };
int run() { printf("%d %d %d\n", DH<std::pair<P, P>>::kind(), DH<P>::kind(), DH<int>::kind()); return 0; }
}

namespace p_ctadexpr {
namespace W {
template<typename C> class PB { public: PB(std::span<const C> s) : n((int)s.size()) { } int n; };
template<typename C> PB(std::span<const C>) -> PB<C>;
template<typename S, typename F> decltype(auto) readChars(S&& s, const F& f) {
    if (s.is8) return f(PB { s.span8() });
    return f(PB { s.span8() });
}
}
struct Str { bool is8 = true; unsigned char d[3] { 1, 2, 3 }; std::span<const unsigned char> span8() const { return { d, 3 }; } };
int run() { Str s; int r = W::readChars(s, [](auto b) { return b.n; }); printf("%d\n", r); return 0; }
}

int main() {
    p_inhctor::run();
    p_inhctor6::run();
    p_partcon::run();
    p_ctadexpr::run();
    return 0;
}
