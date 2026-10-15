// WebKit forms 16: JavaScriptCore's (static members, base-qualified access, folds, library pieces)
#include <stdio.h>
#include <stdint.h>
#include <variant>
#include <type_traits>
#include <compare>
#include <tuple>
#include <optional>
#include <stddef.h>
#include <memory>
#include <string>
#include <iterator>
#include <vector>

namespace p_statt2 {
struct A {};
struct B {};
struct Cell {
    static void visit(Cell *cell, A &a);
    static void visit(Cell *cell, B &b);
    template <typename V> __attribute__((always_inline)) inline static void visitImpl(Cell *cell, V &v);
    int id = 3;
};
template <typename V> void Cell::visitImpl(Cell *cell, V &v) { printf("impl %d %zu\n", cell->id, sizeof(V)); }
inline void Cell::visit(Cell *cell, A &a) { visitImpl(cell, a); }
inline void Cell::visit(Cell *cell, B &b) { visitImpl(cell, b); }
int run() { Cell c; A a; B b; Cell::visit(&c, a); Cell::visit(&c, b); return 0; }
}

namespace p_memalias {
enum PtrTag : uintptr_t { NoPtrTag, JSEntryPtrTag };
template <PtrTag tag> class MacroAssemblerCodeRef { public: int t = (int)tag + 10; };
class JITCode {
public:
    template <PtrTag tag> using CodeRef = MacroAssemblerCodeRef<tag>;
    virtual ~JITCode() {}
    CodeRef<NoPtrTag> base() { return {}; }
};
class JITCodeWithCodeRef : public JITCode {
public:
    JITCodeWithCodeRef(CodeRef<JSEntryPtrTag> r, int x) : v(r.t + x) {}
    CodeRef<JSEntryPtrTag> ref() { return {}; }
    int v;
};
int run() {
    JITCodeWithCodeRef j(JITCode::CodeRef<JSEntryPtrTag>(), 1);
    printf("%d %d %d\n", j.v, j.ref().t, j.base().t);
    return 0;
}
}

namespace p_explinst {
enum Kind { Baseline, Upper };
template <Kind k> class Counter {
public:
    void count() { m_total += (int)k + 1; }
    int total() const { return m_total; }
    int unused() const { return 7; }
private:
    int m_total = 0;
};
extern template class Counter<Baseline>;
template class Counter<Upper>;
typedef Counter<Baseline> BaselineCounter;
int run() {
    Counter<Upper> u;
    u.count(); u.count();
    BaselineCounter b;
    b.count();
    printf("%d %d %d\n", u.total(), b.total(), u.unused());
    return 0;
}
template class Counter<Baseline>;
}

namespace p_nestvar {
struct Payload { int *data; Payload() : data(nullptr) {} Payload(Payload &&o) : data(o.data) { o.data = nullptr; } Payload &operator=(Payload &&o) { data = o.data; o.data = nullptr; return *this; } };
class CacheUpdate {
public:
    struct GlobalUpdate { Payload m_payload; };
    struct FunctionUpdate { long m_base; Payload m_payload; };
    CacheUpdate(GlobalUpdate &&g);
    CacheUpdate(CacheUpdate &&);
    CacheUpdate &operator=(CacheUpdate &&);
    static constexpr bool trivial = std::is_trivially_move_assignable_v<GlobalUpdate>;
    static constexpr bool assignable = std::is_move_assignable_v<GlobalUpdate>;
};
int run() { printf("%d %d\n", CacheUpdate::trivial, CacheUpdate::assignable); return 0; }
}

namespace p_ordinal {
class OrdinalNumber {
public:
    static OrdinalNumber fromZeroBasedInt(int v) { return OrdinalNumber(v); }
    OrdinalNumber() : m_value(0) {}
    int zeroBasedInt() const { return m_value; }
    friend bool operator==(OrdinalNumber, OrdinalNumber) = default;
    friend std::strong_ordering operator<=>(OrdinalNumber, OrdinalNumber) = default;
private:
    OrdinalNumber(int v) : m_value(v) {}
    int m_value;
};
int run() {
    auto a = OrdinalNumber::fromZeroBasedInt(3), b = OrdinalNumber::fromZeroBasedInt(5);
    printf("%d %d %d %d\n", a == b, a != b, a < b, b >= a);
    return 0;
}
}

namespace p_condlv {
struct Impl { int v; Impl(int x) : v(x) {} Impl(const Impl &) = delete; };
template <class T> struct Ref { T *p; Ref(T &t) : p(&t) {} };
static Impl a(1), b(2);
static Impl *empty() { return &a; }
int run() {
    bool c = false;
    Ref<Impl> r(c ? *empty() : *&b);
    Impl &x = c ? a : b;
    (c ? a : b).v = 7;
    printf("%d %d %d\n", r.p->v, x.v, b.v);
    return 0;
}
}

namespace p_foldq {
struct A { static constexpr bool zero = true; static constexpr int size = 2; };
struct B { static constexpr bool zero = false; static constexpr int size = 3; };
template <typename First, typename... Traits> struct Tuple {
    static constexpr bool allZero = First::zero && (Traits::zero && ...);
    static constexpr int total = First::size + (Traits::size + ... + 0);
};
int run() { printf("%d %d %d %d\n", Tuple<A, A>::allZero, Tuple<A, B>::allZero, Tuple<A, A, B>::total, Tuple<B>::allZero); return 0; }
}

namespace p_foldq2 {
template <typename T> struct GenericTraits { static constexpr bool emptyValueIsZero = false; typedef T TraitType; typedef T EmptyValueType; static T emptyValue() { return T(); } };
template <typename T> struct Traits : GenericTraits<T> {};
template <> struct Traits<int> : GenericTraits<int> { static constexpr bool emptyValueIsZero = true; };
template <typename FirstTrait, typename... Others>
struct TupleTraits : GenericTraits<std::tuple<typename FirstTrait::TraitType, typename Others::TraitType...>> {
    typedef std::tuple<typename FirstTrait::TraitType, typename Others::TraitType...> TraitType;
    static constexpr bool emptyValueIsZero = FirstTrait::emptyValueIsZero && (Others::emptyValueIsZero && ...);
    static TraitType emptyValue() { return std::make_tuple(FirstTrait::emptyValue(), Others::emptyValue()...); }
};
int run() {
    printf("%d %d %d\n", TupleTraits<Traits<int>, Traits<int>>::emptyValueIsZero, TupleTraits<Traits<int>, Traits<long>>::emptyValueIsZero, TupleTraits<Traits<int>>::emptyValueIsZero);
    auto t = TupleTraits<Traits<int>, Traits<long>>::emptyValue();
    printf("%d %ld\n", std::get<0>(t), std::get<1>(t));
    return 0;
}
}

namespace p_foldq3 {
template <typename T> struct Tr { typedef T TraitType; };
template <typename... Others> struct TT { typedef std::tuple<typename Others::TraitType...> TraitType; };
int run() { TT<Tr<int>, Tr<char>>::TraitType t { 1, 'a' }; printf("%d %c\n", std::get<0>(t), std::get<1>(t)); return 0; }
}

namespace p_fneq {
struct Cell { static void destroy(Cell *) {} static void other(Cell *) {} };
struct Object : Cell { using Base = Cell; };
struct Array : Object { using Base = Object; int x; };
struct Own : Object { using Base = Object; static void destroy(Cell *) {} };
static_assert(Array::destroy == Array::Base::destroy);
static_assert(Own::destroy != Own::Base::destroy);
static_assert(&Cell::destroy != &Cell::other);
int run() { printf("ok\n"); return 0; }
}

namespace p_injq {
struct PS { enum class Kind { Get = 3, Put }; PS(Kind k) : k(k) {} Kind k; };
int run() {
    PS s(PS::PS::Kind::Put);
    printf("%d\n", (int)s.k);
    return 0;
}
}

namespace p_statinit {
namespace N {
struct Info { const char *name; const Info *parent; };
struct A { static const Info s_info; };
struct B : A { typedef A Base; static const Info s_info; };
const Info A::s_info = { "A", nullptr };
const Info B::s_info = { "B", &Base::s_info };
}
int run() {
    printf("%s %s\n", N::B::s_info.name, N::B::s_info.parent->name);
    return 0;
}
}

namespace p_cx3 {
struct R { int a, b; };
template<typename T> struct Tr { static constexpr std::optional<R> range { std::nullopt }; };
template<> struct Tr<int> { static constexpr std::optional<R> range { { 3, 5 } }; };
struct Info { const char *name; std::optional<R> range; };
const Info ci = { "c", Tr<char>::range };
const Info ii = { "i", Tr<int>::range };
int run() {
    printf("%d %d %d\n", (int)ci.range.has_value(), (int)ii.range.has_value(), ii.range->b);
    return 0;
}
}

namespace p_membspec {
struct Info { const char *name; const Info *parent; };
struct A { static const Info s_info; };
const Info A::s_info = { "A", nullptr };
template<typename P> struct CB : P { typedef P Base; static const Info s_info; };
template<> const Info CB<A>::s_info;
template<> const Info CB<A>::s_info = { "CB<A>", &Base::s_info };
int run() {
    printf("%s %s\n", CB<A>::s_info.name, CB<A>::s_info.parent->name);
    return 0;
}
}

namespace p_tyarg {
template<typename T> constexpr T rup(size_t d, T x) { return (x + d - 1) & ~(T)(d - 1); }
template<size_t d> constexpr size_t rup(size_t x) { return (x + d - 1) & ~(d - 1); }
template<size_t d, typename T> constexpr T *rup(T *x) { return (T *)rup<d>((size_t)x); }
enum class Acc { Main, Conc };
struct VM { int n; };
struct Str {
    static constexpr size_t atomSize = 16;
    template<typename, Acc> static int *subspaceFor(VM &vm) { return &vm.n; }
};
template<typename Type> inline auto subspaceFor(VM &vm) { return Type::template subspaceFor<Type, Acc::Main>(vm); }
template<typename T> size_t alloc(VM &vm, size_t size) {
    return (size_t)*subspaceFor<T>(vm) + rup<T::atomSize>(size);
}
int run() {
    VM vm { 5 };
    printf("%zu %zu\n", alloc<Str>(vm, 20), rup<8>((size_t)13));
    return 0;
}
}

namespace p_tyarg2 {
template<typename T> constexpr T rup(size_t d, T x) { return (x + d - 1) & ~(T)(d - 1); }
template<size_t d> constexpr size_t rup(size_t x) { return (x + d - 1) & ~(d - 1); }
struct Str { static constexpr size_t atomSize = 16; };
template<typename T> size_t alloc(size_t size) { return rup<T::atomSize>(size); }
int run() {
    printf("%zu\n", alloc<Str>(20));
    return 0;
}
}

namespace p_arrowq {
struct Cell { int k; const char *classInfo() const { return "cell"; } };
struct Obj : Cell { const char *classInfo() const { return "obj"; } };
template<typename To, typename From> bool same(From *from) {
    bool canCast = from->Cell::classInfo()[0] == To::name()[0];
    return canCast;
}
struct N { static const char *name() { return "cell"; } };
int run() {
    Obj o;
    Obj *p = &o;
    printf("%s %s %d\n", p->Cell::classInfo(), p->classInfo(), (int)same<N>(p));
    return 0;
}
}

namespace p_uninit {
struct W { int v; W(int x) : v(x * 10) {} W(const W &o) : v(o.v + 1) {} ~W() { printf("~%d ", v); } };
int run() {
    int src[4] = { 1, 2, 3, 4 };
    alignas(W) unsigned char raw[sizeof(W) * 4];
    W *w = reinterpret_cast<W *>(raw);
    W *end = std::uninitialized_copy(src, src + 4, w);
    printf("%d %d %d\n", (int)(end - w), w[0].v, w[3].v);
    std::destroy(w, end);
    std::uninitialized_fill_n(w, 2, W(7));
    printf("\n%d %d\n", w[0].v, w[1].v);
    std::destroy_n(w, 2);
    alignas(std::string) unsigned char sraw[sizeof(std::string) * 2];
    std::string *s = reinterpret_cast<std::string *>(sraw);
    std::string from[2] = { "ab", "cd" };
    std::uninitialized_move(from, from + 2, s);
    printf("\n%s %s\n", s[0].c_str(), s[1].c_str());
    std::destroy(s, s + 2);
    int ints[3];
    std::uninitialized_value_construct_n(ints, 3);
    printf("%d\n", ints[0] + ints[1] + ints[2]);
    return 0;
}
}

namespace p_moveit {
int run() {
    std::vector<std::string> v = { "one", "two", "three" };
    alignas(std::string) unsigned char raw[sizeof(std::string) * 3];
    std::string *d = reinterpret_cast<std::string *>(raw);
    std::uninitialized_copy(std::move_iterator { v.begin() }, std::move_iterator { v.end() }, d);
    printf("%s %s %s [%zu]\n", d[0].c_str(), d[1].c_str(), d[2].c_str(), v[0].size());
    auto it = std::make_move_iterator(v.begin());
    std::string taken = *(it + 1);
    printf("%s %zu %d\n", taken.c_str(), v[1].size(), (int)(std::make_move_iterator(v.end()) - it));
    std::destroy(d, d + 3);
    return 0;
}
}

int main() {
    p_statt2::run();
    p_memalias::run();
    p_explinst::run();
    p_nestvar::run();
    p_ordinal::run();
    p_condlv::run();
    p_foldq::run();
    p_foldq2::run();
    p_foldq3::run();
    p_fneq::run();
    p_injq::run();
    p_statinit::run();
    p_cx3::run();
    p_membspec::run();
    p_tyarg::run();
    p_tyarg2::run();
    p_arrowq::run();
    p_uninit::run();
    p_moveit::run();
    return 0;
}
