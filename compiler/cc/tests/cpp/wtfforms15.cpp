// Forms JavaScriptCore's sources use: <list>, map and set as red-black trees with comparators, constexpr floating point and bit_cast, default template arguments added by a redeclaration, static beside non-static overloads, base copies from derived objects, members in sizeof in the class, enumerators through their enum, inherited default constructors, alias template redeclarations, forward declarations in an inner namespace, injected base names, members hiding a base's, using-declared base overloads, dereference through a pointer conversion, nested class members of class templates, const constexpr constants, deduction from base instances, string moves
#include <list>
#include <stdio.h>
#include <string>
#include <typeinfo>
#include <map>
#include <set>
#include <functional>
#include <bit>
#include <limits>
#include <stdint.h>
#include <limits.h>
#include <stddef.h>
#include <utility>
#include <type_traits>
#include <concepts>
#include <span>

namespace p_lists {
struct Item {
    int id;
    std::string name;
    Item(int i, std::string n) : id(i), name(std::move(n)) {}
};
static void show(const char *label, const std::list<int> &l) {
    printf("%s[%zu]:", label, l.size());
    for (int v : l) printf(" %d", v);
    printf("\n");
}
int run() {
    std::list<int> a { 5, 3, 9, 1 };
    a.push_back(7);
    a.push_front(0);
    show("a", a);
    a.sort();
    show("sorted", a);
    a.reverse();
    show("reversed", a);
    std::list<int> b { 4, 4, 2, 8 };
    b.unique();
    b.remove(8);
    show("b", b);
    auto it = a.begin();
    ++it; ++it;
    a.splice(it, b);
    show("spliced", a);
    show("b after", b);
    a.remove_if([](int v) { return v % 3 == 0; });
    show("no threes", a);
    a.erase(a.begin());
    a.insert(a.end(), 2, 11);
    show("edited", a);
    std::list<int> c(a);
    c.pop_back();
    printf("%d %d %d %d\n", a == c, a != c, c.front(), c.back());
    std::list<int> d = std::move(c);
    printf("%zu %zu\n", c.size(), d.size());
    for (auto r = d.rbegin(); r != d.rend(); ++r) printf("%d ", *r);
    printf("\n");
    std::list<Item> items;
    items.emplace_back(1, "one");
    items.emplace_front(0, "zero");
    for (const auto &item : items) printf("%d=%s ", item.id, item.name.c_str());
    printf("\n");
    std::list<int> x { 1, 4, 6 }, y { 2, 3, 5, 7 };
    x.merge(y);
    show("merged", x);
    std::list<int>::const_iterator ci = x.begin();
    printf("%d %zu\n", *ci, y.size());
    return 0;
}
}

namespace p_trees {
static unsigned state = 12345;
static unsigned next() { state = state * 1103515245u + 12345u; return (state >> 8) & 0xffff; }
int run() {
    std::map<int, int> m;
    std::set<int, std::greater<int>> s;
    std::multiset<int> ms;
    long check = 0;
    for (int i = 0; i < 20000; ++i) {
        int k = next() % 500;
        int op = next() % 4;
        if (op == 0) { m[k] += i; s.insert(k); ms.insert(k); }
        else if (op == 1) { check += m.erase(k) * 3 + s.erase(k) * 5; auto it = ms.find(k); if (it != ms.end()) ms.erase(it); }
        else if (op == 2) { auto it = m.lower_bound(k); if (it != m.end()) check += it->first * 7 + it->second; }
        else { auto it = s.upper_bound(k); if (it != s.end()) check += *it; check += ms.count(k); }
    }
    long sum = 0;
    for (auto &[k, v] : m) sum = sum * 31 + k * 3 + v;
    for (int k : s) sum = sum * 7 + k;
    for (auto it = ms.rbegin(); it != ms.rend(); ++it) sum = sum * 3 + *it;
    printf("%zu %zu %zu %ld %ld\n", m.size(), s.size(), ms.size(), check, sum);
    std::map<std::string, std::string> names { { "b", "bee" }, { "a", "ay" } };
    names.try_emplace("c", "see");
    names.insert_or_assign("a", "eh");
    auto [where, inserted] = names.insert({ "b", "no" });
    printf("%s %d %zu\n", where->second.c_str(), inserted, names.count("c"));
    for (const auto &p : names) printf("%s=%s ", p.first.c_str(), p.second.c_str());
    printf("\n");
    std::map<int, int> copy = m;
    copy.erase(copy.begin(), copy.find(copy.rbegin()->first));
    printf("%zu %d %d\n", copy.size(), copy == m, std::map<int,int>(m) == m);
    std::multimap<int, std::string> mm;
    mm.insert({ 2, "two" });
    mm.emplace(1, "one");
    mm.insert({ 2, "deux" });
    auto range = mm.equal_range(2);
    for (auto it = range.first; it != range.second; ++it) printf("%s ", it->second.c_str());
    printf("%zu\n", mm.count(2));
    return 0;
}
}

namespace p_nan {
constexpr uint64_t bitsOf(double v) { return std::bit_cast<uint64_t>(v); }
static_assert(bitsOf(1.0) == 0x3ff0000000000000ULL);
static_assert(bitsOf(-2.0) == 0xc000000000000000ULL);
constexpr double PNaN = std::bit_cast<double>(0x7ff8000000000000ULL);
static_assert(bitsOf(PNaN) == 0x7ff8000000000000ULL);
static_assert(bitsOf(-PNaN) == 0xfff8000000000000ULL);
static_assert(bitsOf(std::numeric_limits<double>::quiet_NaN()) == 0x7ff8000000000000ULL);
static_assert(bitsOf(-std::numeric_limits<double>::quiet_NaN()) == 0xfff8000000000000ULL);
static_assert(std::bit_cast<uint32_t>(1.0f) == 0x3f800000u);
int run() { printf("%llx\n", (unsigned long long)bitsOf(PNaN)); return 0; }
}

namespace p_nan2 {
constexpr uint64_t direct(double v) { return __builtin_bit_cast(uint64_t, v); }
constexpr uint64_t viaRef(const double &v) { return __builtin_bit_cast(uint64_t, v); }
constexpr double twice(double v) { return v * 2; }
constexpr bool less(double a, double b) { return a < b; }
constexpr int trunc(double v) { return (int)v; }
template <uint64_t V> struct Show { static constexpr uint64_t value = V; };
int run() {
    printf("%llx\n", (unsigned long long)Show<direct(1.0)>::value);
    printf("%llx\n", (unsigned long long)Show<viaRef(1.0)>::value);
    printf("%llx\n", (unsigned long long)Show<std::bit_cast<uint64_t>(1.0)>::value);
    printf("%llx\n", (unsigned long long)Show<direct(twice(1.5))>::value);
    printf("%d %d\n", (int)Show<less(1.0, 2.0)>::value, (int)Show<trunc(7.9)>::value);
    return 0;
}
}

namespace p_pnan {
static constexpr unsigned JSValueDoubleEncodeOffsetBit { 49 };
static constexpr uint64_t JSValueDoubleEncodeOffset { 1ULL << JSValueDoubleEncodeOffsetBit };
static constexpr uint64_t JSValueNumberTag { 0xfffe000000000000ULL };
constexpr double PNaN = std::bit_cast<double>(0x7ff8000000000000ll);
inline constexpr bool isImpureNaN(double value)
{
    uint64_t bits = std::bit_cast<uint64_t>(value);
    bits += JSValueDoubleEncodeOffset;
    uint64_t mask = bits & JSValueNumberTag;
    return !mask || mask == JSValueNumberTag;
}
static_assert(!isImpureNaN(std::numeric_limits<double>::quiet_NaN()));
static_assert(!isImpureNaN(-std::numeric_limits<double>::quiet_NaN()));
static_assert(!isImpureNaN(PNaN));
static_assert(!isImpureNaN(-PNaN));
int run() { printf("%d\n", isImpureNaN(PNaN)); return 0; }
}

namespace p_redecl {
template <class T, typename Traits> class Holder;
template <class T> struct DefaultTraits { static constexpr int kind = sizeof(T); };
template <class T, typename Traits = DefaultTraits<T>> class Holder;
template <> class Holder<char>;
template <class T, typename Traits> class Holder {
public:
    int kind() const { return Traits::kind; }
};
template <> class Holder<char> {
public:
    int kind() const { return 99; }
};
template <class A, class B, class C> struct Three;
template <class A, class B, class C = int> struct Three;
template <class A, class B = long, class C> struct Three { static constexpr int size = sizeof(A) + sizeof(B) + sizeof(C); };
int run() {
    Holder<int> a;
    Holder<double> b;
    Holder<char> c;
    printf("%d %d %d %d\n", a.kind(), b.kind(), c.kind(), Three<char>::size);
    return 0;
}
}

namespace p_statov {
class PageCount {
public:
    PageCount(uint64_t n) : m_count(n) {}
    static bool isValid(uint64_t n) { return n <= 100; }
    bool isValid() const { return isValid(m_count); }
    static PageCount from(uint64_t n)
    {
        PageCount count(n);
        printf("%d %d\n", count.isValid(), isValid(n * 2));
        return count;
    }
    uint64_t m_count;
};
int run() {
    PageCount a = PageCount::from(60);
    printf("%d %d\n", a.isValid(), PageCount::isValid(7));
    return 0;
}
}

namespace p_basecopy {
struct Base {
    std::string name;
    Base() : name("none") {}
    explicit Base(const char *n) : name(n) {}
};
struct Derived : Base {
    Derived() {}
    Derived(const char *n) : Base(n) {}
    Derived(const Derived &other) : Base(other) {}
    Derived(Derived &&other) : Base(static_cast<Base &&>(other)) {}
};
struct Plain { int x; Plain() : x(3) {} };
struct FromPlain : Plain { FromPlain() {} FromPlain(const FromPlain &o) : Plain(o) {} };
int run() {
    Derived a("alpha");
    Derived b(a);
    Derived c(static_cast<Derived &&>(b));
    FromPlain p; p.x = 9; FromPlain q(p);
    printf("%s %s %s %d\n", a.name.c_str(), b.name.c_str(), c.name.c_str(), q.x);
    return 0;
}
}

namespace p_sizeofmem {
class Random {
public:
    uint64_t m_triggerBits;
    short m_small[3];
    static constexpr int numberOfTriggerBits = (sizeof(m_triggerBits) * CHAR_BIT) - 1;
    static constexpr int smallBytes = sizeof(m_small);
    using BitsType = decltype(m_triggerBits);
};
int run() {
    printf("%d %d %zu\n", Random::numberOfTriggerBits, Random::smallBytes, sizeof(Random::BitsType));
    return 0;
}
}

namespace p_enumq {
class MacroAssembler {
public:
    enum RegisterID : int8_t { NoRegister, InvalidGPRReg = -1 };
    enum FPRegisterID : int8_t { NoFPRegister, InvalidFPRReg = -1 };
};
typedef MacroAssembler::RegisterID GPRReg;
using FPRReg = MacroAssembler::FPRegisterID;
static constexpr GPRReg InvalidGPRReg { GPRReg::InvalidGPRReg };
static constexpr FPRReg InvalidFPRReg { FPRReg::InvalidFPRReg };
enum Plain { A = 5 };
int run() {
    printf("%d %d %d %d\n", (int)InvalidGPRReg, (int)InvalidFPRReg, (int)MacroAssembler::RegisterID::NoRegister, (int)Plain::A);
    return 0;
}
}

namespace p_inhdef {
class PageBlock {
public:
    PageBlock() = default;
    PageBlock(void *b, size_t s) : m_base(b), m_size(s) {}
    size_t size() const { return m_size; }
    void *base() const { return m_base; }
    explicit operator bool() const { return !!m_base; }
private:
    void *m_base { nullptr };
    size_t m_size { 0 };
};
class PageAllocation : private PageBlock {
public:
    using PageBlock::PageBlock;
    using PageBlock::size;
    using PageBlock::base;
    using PageBlock::operator bool;
    void deallocate() { PageAllocation tmp; std::swap(tmp, *this); }
    static PageAllocation make(size_t n) { static char area[64]; return PageAllocation(area, n); }
};
int run() {
    PageAllocation a = PageAllocation::make(16);
    PageAllocation tmp;
    std::swap(tmp, a);
    printf("%zu %d %zu %d\n", tmp.size(), (bool)tmp, a.size(), (bool)a);
    tmp.deallocate();
    printf("%zu\n", tmp.size());
    return 0;
}
}

namespace p_aliasre {
template <class T, class Traits, class Raw> struct Generic { static constexpr int size = sizeof(Raw); };
template <class Raw> struct Traits {};
template <typename T, typename Raw = uint64_t> using Atomic = Generic<T, Traits<Raw>, Raw>;
template <typename T, typename Raw> using Atomic = Generic<T, Traits<Raw>, Raw>;
enum Tag {};
using Id = Atomic<Tag>;
int run() { printf("%d %d\n", Id::size, Atomic<Tag, char>::size); return 0; }
}

namespace p_nsq {
namespace JSC {
class GetterSetter { public: int g; };
struct ClassInfo { int x; };
namespace DOMJIT { class GetterSetter; }
struct DOMAttributeAnnotation {
    const ClassInfo* classInfo;
    const DOMJIT::GetterSetter* domJIT;
    friend bool operator==(const DOMAttributeAnnotation&, const DOMAttributeAnnotation&) = default;
};
namespace DOMJIT { class GetterSetter { public: int h; }; }
}
int run() {
    JSC::ClassInfo c;
    JSC::DOMAttributeAnnotation a { &c, nullptr }, b { &c, nullptr };
    printf("%d\n", a == b);
    return 0;
}
}

namespace p_injbase {
template <typename T> class GenericOffset {
public:
    GenericOffset() : m_offset(~0u) {}
    explicit GenericOffset(unsigned offset) : m_offset(offset) {}
    unsigned offset() const { return m_offset; }
protected:
    unsigned m_offset;
};
class ScopeOffset : public GenericOffset<ScopeOffset> {
public:
    ScopeOffset() {}
    explicit ScopeOffset(unsigned offset) : GenericOffset(offset) {}
};
template <class T> class Derived : public GenericOffset<Derived<T>> {
public:
    Derived(unsigned o) : GenericOffset<Derived<T>>(o * 2) {}
};
int run() {
    ScopeOffset a(7), b;
    Derived<int> d(5);
    printf("%u %u %u\n", a.offset(), b.offset(), d.offset());
    return 0;
}
}

namespace p_hide {
struct Blob { unsigned w; unsigned blob() const { return w * 2; } };
struct Cell { int id; union { unsigned m_blob; struct { unsigned char a, b, c, d; }; }; };
struct Structure : Cell {
    Blob m_blob;
    unsigned get() const { return m_blob.blob(); }
    unsigned base() const { return Cell::m_blob; }
};
int run() {
    Structure s;
    s.id = 1;
    s.Cell::m_blob = 0x01020304;
    s.m_blob.w = 21;
    printf("%u %x %u %u\n", s.get(), s.base(), s.a, s.m_blob.w);
    return 0;
}
}

namespace p_usingov2 {
struct Pad { long a, b; };
struct Cell { int tag = 42; void finish(int x) { printf("cell %d %d\n", tag, x); } int value() const { return tag; } };
struct Str : Pad, Cell {
    using Cell::finish;
    void finish(int x, int y) { printf("str %d %d %d\n", tag, x, y); }
    using Cell::value;
    int value(int k) const { return tag + k; }
};
int run() {
    Str s;
    s.finish(1);
    s.finish(2, 3);
    Str *p = &s;
    p->finish(4);
    printf("%d %d\n", s.value(), p->value(8));
    return 0;
}
}

namespace p_convderef {
template <typename T> struct Scope {
    operator const T() const { return data; }
    operator T() { return data; }
    T data;
};
struct Impl { int v; };
int run() {
    Impl i { 9 };
    Scope<Impl *> s { &i };
    Impl &r = *s;
    const Scope<Impl *> c { &i };
    printf("%d %d\n", r.v, (*c).v);
    return 0;
}
}

namespace p_nestdef {
template <typename T> class Inferred {
public:
    class WatchSet {
    public:
        void notify(T *value, const char *reason);
        T *m_value = nullptr;
        int count = 0;
    };
    WatchSet set;
};
template <typename T>
void Inferred<T>::WatchSet::notify(T *value, const char *reason)
{
    m_value = value;
    count++;
    printf("%s %d %d\n", reason, *value, count);
}
int run() {
    Inferred<int> i;
    int v = 5;
    i.set.notify(&v, "first");
    i.set.notify(&v, "second");
    return 0;
}
}

namespace p_cxconst {
struct V { static constexpr bool safe = true; static constexpr int n { 4 }; };
constexpr long K = 7;
static_assert(std::same_as<decltype(V::safe), const bool>);
static_assert(std::is_same_v<decltype(V::n), const int>);
static_assert(std::is_same_v<decltype(K), const long>);
int run() { auto a = V::n; a++; int arr[K]; printf("%d %zu %d\n", a, sizeof(arr), V::safe); return 0; }
}

namespace p_statnon {
struct G {
    int s = 3;
    int *structure() { return &s; }
    static int *structure(G *) { return nullptr; }
};
int run() { G g; printf("%d %d\n", *g.structure(), G::structure(&g) == nullptr); return 0; }
}

namespace p_deducebase {
struct Unknown {};
template <class T> struct RawValueTraits {};
template <class T, class Traits> class WriteBarrierBase { public: int id = sizeof(T); };
template <class T, class Traits> class WriteBarrier : public WriteBarrierBase<T, Traits> {};
template <> class WriteBarrier<Unknown, RawValueTraits<Unknown>> : public WriteBarrierBase<Unknown, RawValueTraits<Unknown>> { public: int extra = 5; };
struct Visitor {
    template <typename T, typename Traits> void append(const WriteBarrierBase<T, Traits> &b) { printf("barrier %d\n", b.id); }
    void append(int x) { printf("int %d\n", x); }
    void appendValues(std::span<const WriteBarrier<Unknown, RawValueTraits<Unknown>>> barriers) {
        for (auto &barrier : barriers) append(barrier);
    }
};
int run() {
    Visitor v;
    WriteBarrier<Unknown, RawValueTraits<Unknown>> arr[2];
    v.appendValues(arr);
    WriteBarrier<long, RawValueTraits<long>> l;
    v.append(l);
    v.append(3);
    return 0;
}
}

namespace p_specdef {
struct Unknown {};
struct JSValue { long bits; };
template <class T> struct RawPtrTraits { static constexpr int kind = 1; };
template <class T> struct RawValueTraits { static constexpr int kind = 2; };
template <class T>
using TraitsSelect = typename std::conditional<std::is_same<T, Unknown>::value, RawValueTraits<T>, RawPtrTraits<T>>::type;
template <class T, typename Traits = TraitsSelect<T>> class Base;
template <> class Base<JSValue>;
template <class T, typename Traits> class Base {
public:
    int kind() const { return Traits::kind; }
};
template <> class Base<JSValue> {
public:
    int kind() const { return 7; }
};
template <class T, typename Traits = TraitsSelect<T>> class Barrier : public Base<T, Traits> {};
int run() {
    Base<int> a;
    Base<Unknown> b;
    Base<JSValue> c;
    Barrier<JSValue> d;
    Barrier<Unknown> e;
    printf("%d %d %d %d %d\n", a.kind(), b.kind(), c.kind(), d.kind(), e.kind());
    return 0;
}
}

namespace p_mv {
struct Base { std::string name; Base() {} explicit Base(const char *n) : name(n) {} };
struct Derived : Base { Derived(const char *n) : Base(n) {} };
int run() {
    Base a("one");
    Base b(static_cast<Base &&>(a));
    Derived d("two");
    Base c(static_cast<Base &&>(d));
    Derived e("three");
    Base f(std::move(e));
    printf("[%s] [%s] [%s] [%s] [%s] [%s]\n", a.name.c_str(), b.name.c_str(), d.name.c_str(), c.name.c_str(), e.name.c_str(), f.name.c_str());
    return 0;
}
}

namespace p_mv2 {
struct P { std::string s; };
struct Q { std::string s; Q() {} };
int run() {
    P p1 { "one" };
    P p2(std::move(p1));
    P p3 { "three" };
    P p4 = std::move(p3);
    Q q1; q1.s = "q";
    Q q2(std::move(q1));
    Q q3; q3.s = "r";
    Q q4 = std::move(q3);
    printf("[%s] [%s] [%s] [%s] [%s] [%s] [%s] [%s]\n", p1.s.c_str(), p2.s.c_str(), p3.s.c_str(), p4.s.c_str(), q1.s.c_str(), q2.s.c_str(), q3.s.c_str(), q4.s.c_str());
    return 0;
}
}

int main() {
    p_lists::run();
    p_trees::run();
    p_nan::run();
    p_nan2::run();
    p_pnan::run();
    p_redecl::run();
    p_statov::run();
    p_basecopy::run();
    p_sizeofmem::run();
    p_enumq::run();
    p_inhdef::run();
    p_aliasre::run();
    p_nsq::run();
    p_injbase::run();
    p_hide::run();
    p_usingov2::run();
    p_convderef::run();
    p_nestdef::run();
    p_cxconst::run();
    p_statnon::run();
    p_deducebase::run();
    p_specdef::run();
    p_mv::run();
    p_mv2::run();
    return 0;
}
