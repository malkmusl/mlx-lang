// WebKit forms 20: JavaScriptCore's (function traits over auto function arguments, class operator new with placement arguments, constexpr condition declarations, lambda and specialization attributes, member templates of base and instance, template-argument ADL, optional ordering, typedef'd enums and tags as qualifiers, copy elision of temporaries)
#include <stdio.h>
#include <tuple>
#include <type_traits>
#include <stdlib.h>
#include <stddef.h>
#include <new>
#include <utility>
#include <optional>
#include <map>
#include <string>

namespace p_ftraits {
template<typename T> struct FT;
template<typename Result, typename... Args> struct FT<Result(Args...)> {
    using ResultType = Result;
    static constexpr std::size_t arity = sizeof...(Args);
    template<std::size_t n> requires (n < arity)
    using ArgumentType = typename std::tuple_element<n, std::tuple<Args...>>::type;
};
template<typename Result, typename... Args> struct FT<Result(*)(Args...)> : public FT<Result(Args...)> {};
template<typename Result, typename... Args> struct FT<Result(Args...) noexcept> : public FT<Result(Args...)> {};
template<typename Result, typename... Args> struct FT<Result(*)(Args...) noexcept> : public FT<Result(Args...)> {};
extern "C" void my_free(void *p) { printf("free %p\n", p != nullptr ? (void *)1 : nullptr); }
struct UCol;
extern "C" void ucol_close(UCol *c) { printf("ucol %d\n", c != nullptr); }
template<auto deleteFunction> struct Deleter {
    void operator()(typename FT<decltype(deleteFunction)>::template ArgumentType<0> pointer) { if (pointer) deleteFunction(pointer); }
};
int run() {
    int x;
    Deleter<my_free>()(&x);
    Deleter<ucol_close>()((UCol *)&x);
    printf("%zu\n", FT<decltype(&my_free)>::arity);
    return 0;
}
}

namespace p_ft3 {
template<typename T> struct FT;
template<typename R, typename... Args> struct FT<R(Args...)> {
    template<std::size_t n> using ArgumentType = typename std::tuple_element<n, std::tuple<Args...>>::type;
};
template<typename R, typename... Args> struct FT<R(*)(Args...)> : FT<R(Args...)> {};
void g(int *p) { printf("g %d\n", *p); }
template<typename F> struct D1 { void operator()(typename FT<F>::template ArgumentType<0> p) { printf("D1 %d\n", *p); } };
template<auto fn> struct D2 { void operator()(typename FT<decltype(fn)>::template ArgumentType<0> p) { fn(p); } };
int run() { int v = 3; D1<void(*)(int*)>()(&v); D2<g>()(&v); return 0; }
}

namespace p_opnew {
struct Arena { int count = 0; };
struct Deletable {
    template<typename T> void *operator new(size_t size, Arena &arena) { arena.count++; return malloc(size); }
    void *operator new(size_t size) { return malloc(size); }
    void operator delete(void *p) { free(p); }
};
struct Node : Deletable {
    void *operator new(size_t size, Arena &arena) { return Deletable::operator new<Node>(size, arena); }
    int v = 9;
};
int run() { Arena a; Node *n = new (a) Node; printf("%d %d\n", n->v, a.count); delete n; return 0; }
}

namespace p_clsnttp {
struct Info { unsigned char storage; constexpr Info(unsigned char s) : storage(s) {} };
template<Info repatch> void *copyCode(void *dst, const void *src, unsigned long n);
template<Info repatch> struct Patcher { static int flags() { return repatch.storage; } };
int run() { printf("ok\n"); return 0; }
}

namespace p_ifconstexpr {
struct Frame { void *a; void *b; void *c; };
constexpr unsigned adjust() {
    if (constexpr unsigned excess = sizeof(Frame) % 16)
        return 16 - excess;
    return 0;
}
static_assert(adjust() == 8);
int run() { if (const int k = 3) printf("%u %d\n", adjust(), k); return 0; }
}

namespace p_misc3 {
constexpr double maxSafe() { return 9007199254740991.0; }
static_assert(maxSafe() < 1e+21);
static_assert(!(maxSafe() > 1e+21));
static_assert(1.5 == 1.5 && 2.5 != 1.0 && 0.5 <= 0.5);
template<typename T> struct Traits { static int kind() { return 0; } };
template<> struct __attribute__((visibility("default"))) Traits<int> { static int kind() { return 1; } };
template<typename F> int call(F f) { return f(4); }
int run() {
    int base = 3;
    int r = call([&](int x) __attribute__((always_inline)) { return x + base; });
    printf("%d %d %d\n", Traits<int>::kind(), Traits<char>::kind(), r);
    return 0;
}
}

namespace p_mtctor {
template<int I> struct idx { explicit idx() = default; };
template<typename... Ts> class base {
public:
    template<int I, typename... Args> explicit constexpr base(idx<I>, Args &&... args) : index_(I) {
        ::new (static_cast<void *>(data_.storage_)) double(std::forward<Args>(args)...);
    }
    struct data_t { unsigned char storage_[16]; } data_;
    int index_;
};
template<typename... Ts> class impl : public base<Ts...> { public: using base<Ts...>::base; };
template<typename... Ts> class var {
public:
    template<typename Arg> constexpr var(Arg &&arg) : impl_(idx<0>{}, std::forward<Arg>(arg)) {}
    impl<Ts...> impl_;
};
struct Value {
    Value() = default;
    explicit Value(double d) : m_value(d) {}
    var<double, int> m_value { 0.0 };
};
int run() { Value v(2.5); Value w; printf("%d %g %g\n", v.m_value.impl_.index_, *(double *)v.m_value.impl_.data_.storage_, *(double *)w.m_value.impl_.data_.storage_); return 0; }
}

namespace p_mtctor5 {
template<int I> struct idx { explicit idx() = default; };
template<typename... Ts> class base {
public:
    template<int I, typename... Args> explicit constexpr base(idx<I>, Args &&... args) : index_(I) {
        ::new (static_cast<void *>(data_.storage_)) double(std::forward<Args>(args)...);
    }
    struct data_t { unsigned char storage_[16]; } data_;
    int index_;
};
template<typename... Ts> class impl : public base<Ts...> { public: using base<Ts...>::base; };
template<typename... Ts> class var {
public:
    template<typename Arg> constexpr var(Arg &&arg) : impl_(idx<0>{}, std::forward<Arg>(arg)) {}
    impl<Ts...> impl_;
};
struct Value { Value() {} var<double, int> m_value { 0.5 }; };
int run() { Value v; printf("%g\n", *(double *)v.m_value.impl_.data_.storage_); return 0; }
}

namespace p_specmt {
template<typename T> class Lexer {
public:
    template<bool create> int parseKeyword(int data);
    int run() { return parseKeyword<true>(3) + parseKeyword<false>(1); }
};
template<> template<bool create> int Lexer<char16_t>::parseKeyword(int data) { return create ? data * 10 : data; }
template<> template<bool create> int Lexer<char>::parseKeyword(int data) { return create ? data * 100 : data + 1; }
int run() { Lexer<char16_t> a; Lexer<char> b; printf("%d %d\n", a.run(), b.run()); return 0; }
}

namespace p_tagqual {
namespace WTF { class MonotonicTime; }
using WTF::MonotonicTime;
namespace WTF { class MonotonicTime { public: static MonotonicTime now() { MonotonicTime t; t.v = 7; return t; } int v = 0; }; }
namespace JSC { struct T { int f() { MonotonicTime now = MonotonicTime::now(); return now.v; } }; }
int run() { JSC::T t; printf("%d\n", t.f()); return 0; }
}

namespace p_adlnot {
namespace WTF {
struct String { const char *p = nullptr; bool isNull() const { return !p; } };
inline bool operator!(const String &s) { return s.isNull(); }
}
using WTF::String;
namespace JSC {
template<typename T> struct Scope {
    operator const T() const { return data; }
    operator T() { return data; }
    T data;
};
Scope<const String &> valueOf(const String &s) { return Scope<const String &> { s }; }
}
int run() {
    String a, b { "x" };
    auto sa = JSC::valueOf(a);
    auto sb = JSC::valueOf(b);
    printf("%d %d\n", (int)!sa, (int)!sb);
    if (!sb) return 1;
    return 0;
}
}

namespace p_optcmp {
int run() {
    std::optional<unsigned> none, three { 3u }, five { 5u };
    unsigned four = 4;
    printf("%d %d %d %d %d %d\n", (int)(three < four), (int)(five < four), (int)(none < four), (int)(four < five), (int)(three < five), (int)(none < three));
    printf("%d %d %d %d\n", (int)(five >= four), (int)(three <= 3u), (int)(4u == five), (int)(three > none));
    return 0;
}
}

namespace p_anonenum {
typedef enum { kUndef, kEqual, kGreater } JSRel;
namespace JSC { JSRel compare(int a, int b) { if (a == b) return JSRel::kEqual; return JSRel::kGreater; } }
int run() { printf("%d %d\n", (int)JSC::compare(1, 1), (int)JSC::compare(2, 1)); return 0; }
}

namespace p_autoname {
struct Sparse {
    typedef std::map<int, int> Map;
    typedef Map::iterator iterator;
    Map m_set;
    int lookup(int i) { auto iterator = m_set.find(i); return iterator == m_set.end() ? -1 : iterator->second; }
};
int run() { Sparse s; s.m_set[3] = 30; printf("%d %d\n", s.lookup(3), s.lookup(4)); return 0; }
}

namespace p_tdspec {
struct Info { const char *name; const Info *parent; };
struct Base { static const Info s_info; };
const Info Base::s_info = { "Base", nullptr };
template<typename T> struct Ctor : Base { typedef Base Base_; static const Info s_info; };
struct Int8 {};
typedef Ctor<Int8> JSInt8Ctor;
template<> const Info JSInt8Ctor::s_info = { "Function", &JSInt8Ctor::Base_::s_info };
int run() { printf("%s %s\n", JSInt8Ctor::s_info.name, JSInt8Ctor::s_info.parent->name); return 0; }
}

namespace p_ovlspec {
struct Regs { void *a; void *b; };
struct Ctx { void *x; void *y; };
template<unsigned long N> void *&argPtr(Regs &);
template<unsigned long N> void *&argPtr(Ctx &);
template<> void *&argPtr<1>(Ctx &);
template<> inline void *&argPtr<1>(Regs &r) { return r.b; }
template<> inline void *&argPtr<1>(Ctx &c) { return c.y; }
int run() { int k = 5; Regs r { 0, &k }; Ctx c { 0, &k }; printf("%d %d\n", *(int *)argPtr<1>(r), *(int *)argPtr<1>(c)); return 0; }
}

namespace p_aggcopy {
struct Ident { std::string s; Ident(const char *p) : s(p) {} };
struct Request { Ident specifier; int kind; };
int run() {
    Request r { Ident("lib"), 2 };
    Request copy { r };
    const Request &ref = r;
    Request copy2 { ref };
    printf("%s %d %s\n", copy.specifier.s.c_str(), copy.kind, copy2.specifier.s.c_str());
    return 0;
}
}

namespace p_aggc6 {
struct Id { int v; Id(int x) : v(x) { printf("ctor %d\n", v); } Id(const Id &o) : v(o.v) { printf("copy %d\n", v); } ~Id() { printf("dtor %d\n", v); } };
struct Req { Id spec; int kind; };
struct Holder { Holder() : spec(Id(5)) {} Id spec; };
int run() {
    { Id a = Id(7); printf("a\n"); }
    { Id b(Id(8)); printf("b\n"); }
    { Req r { Id(9), 2 }; printf("r\n"); }
    { Holder h; printf("h\n"); }
    return 0;
}
}

int main() {
    p_ftraits::run();
    p_ft3::run();
    p_opnew::run();
    p_clsnttp::run();
    p_ifconstexpr::run();
    p_misc3::run();
    p_mtctor::run();
    p_mtctor5::run();
    p_specmt::run();
    p_tagqual::run();
    p_adlnot::run();
    p_optcmp::run();
    p_anonenum::run();
    p_autoname::run();
    p_tdspec::run();
    p_ovlspec::run();
    p_aggcopy::run();
    p_aggc6::run();
    return 0;
}
