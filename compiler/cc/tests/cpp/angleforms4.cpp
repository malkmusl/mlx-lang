// ANGLE forms 4: argument-dependent lookup through a template's
// arguments and through a base class, `new T{x}` choosing among two
// initializer_list constructors, constexpr locals of class and double
// type read by static_assert, `const char *message...`, braces given to
// an implicit copy constructor, a list of elements in braces for an
// initializer_list, a conversion operator template to `const long &`,
// nullptr for a pointer parameter naming no template parameter, raw
// string literals, arrays of a union and of defaulted classes, a
// conversion looked up in a class with a base met twice.
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <array>
#include <initializer_list>
#include <limits>
#include <map>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace p_adl {
namespace pp {
struct Macro { int value; };
typedef std::map<std::string, std::shared_ptr<Macro>> MacroSet;
int countMacros(const MacroSet *set) { return (int)set->size(); }
}
template <class T> struct Base { std::vector<T> items; };
template <class T> bool operator==(const Base<T> &a, const Base<T> &b) { return a.items == b.items; }
namespace other {
template <class T> struct Derived : Base<T> {};
}
struct Chain {
    bool operator==(const Chain &o) const { return d == o.d; }
    other::Derived<int> d;
};
int run() {
    pp::MacroSet set;
    set["A"] = std::make_shared<pp::Macro>();
    Chain a, b;
    a.d.items.push_back(1);
    b.d.items.push_back(1);
    printf("%d %d\n", countMacros(&set), (int)(a == b));
    return 0;
}
}

namespace p_newlist {
struct Variable { int id; };
struct Typed { int id; };
struct Declaration {
    Declaration(std::initializer_list<const Variable *> vars) : kind(1), count((int)vars.size()) {}
    Declaration(std::initializer_list<Typed *> typed) : kind(2), count((int)typed.size()) {}
    int kind, count;
};
int run() {
    Variable v{3};
    Typed t{4};
    Declaration *a = new Declaration{&v};
    Declaration *b = new Declaration{&t, &t};
    printf("%d %d %d %d\n", a->kind, a->count, b->kind, b->count);
    delete a;
    delete b;
    return 0;
}
}

namespace p_constexprlocal {
template <typename T> inline float normalized(T input) {
    constexpr double inverseMax = 1.0 / std::numeric_limits<T>::max();
    static_assert(static_cast<float>(std::numeric_limits<T>::min() * inverseMax) <= -1.0f);
    return static_cast<float>(input * inverseMax);
}
struct Bits {
    constexpr bool valid() const { return red + green < 64; }
    unsigned red, green;
};
template <unsigned R, unsigned G> constexpr Bits make() {
    constexpr Bits bits = {R, G};
    static_assert(bits.valid(), "invalid bits");
    return bits;
}
int run() {
    printf("%g %u\n", normalized<int>(1 << 30), make<8, 16>().green);
    return 0;
}
}

namespace p_varargs {
static int sum(const char *format...) {
    va_list args;
    va_start(args, format);
    int total = 0;
    for (const char *p = format; *p; p++) total += va_arg(args, int);
    va_end(args);
    return total;
}
int run() { printf("%d\n", sum("xyz", 1, 2, 3)); return 0; }
}

namespace p_bracedcopy {
template <typename T> struct Color {
    Color();
    Color(T r, T g, T b, T a);
    T red, green, blue, alpha;
};
template <typename T> Color<T>::Color() : red(0), green(0), blue(0), alpha(0) {}
template <typename T> Color<T>::Color(T r, T g, T b, T a) : red(r), green(g), blue(b), alpha(a) {}
struct State {
    State() : color({1.0f, 2.0f, 3.0f, 4.0f}), count(2) {}
    Color<float> color;
    int count;
};
int run() {
    State s;
    printf("%g %g %d\n", s.color.green, s.color.alpha, s.count);
    return 0;
}
}

namespace p_elementlist {
enum class Binding { A, B, C };
struct Machine {
    typedef void (Machine::*Setter)(int);
    template <Binding B> void set(int x) { total += x * ((int)B + 1); }
    int total = 0;
};
template <Binding B> constexpr std::pair<Binding, Machine::Setter> setter() { return std::make_pair(B, &Machine::set<B>); }
template <typename E, typename T> struct EnumMap {
    using InitPair = std::pair<E, T>;
    constexpr EnumMap(std::initializer_list<InitPair> init) : data{} {
        for (const InitPair *it = init.begin(); it != init.end(); ++it) data[static_cast<int>(it->first)] = it->second;
    }
    std::array<T, 3> data;
};
static const EnumMap<Binding, Machine::Setter> kSetters = {{
    setter<Binding::A>(),
    setter<Binding::B>(),
    setter<Binding::C>(),
}};
int run() {
    Machine m;
    for (int i = 0; i < 3; i++) (m.*kSetters.data[i])(2);
    printf("%d\n", m.total);
    return 0;
}
}

namespace p_conversionref {
template <typename T> class Strict {
  public:
    constexpr Strict(T value) : value_(value) {}
    template <typename Dst, typename std::enable_if<std::is_arithmetic<Dst>::value>::type * = nullptr>
    constexpr operator Dst() const { return static_cast<Dst>(value_); }
  private:
    const T value_;
};
template <typename T> const T &smaller(const T &a, const T &b) { return b < a ? b : a; }
int run() {
    long limit = 100;
    printf("%ld\n", smaller<long>(limit, Strict<long>(7)));
    return 0;
}
}

namespace p_nullptrparam {
template <typename B> int append(const std::vector<B> &in, std::array<unsigned, 4> *map) { return (int)in.size() + (map ? 10 : 20); }
int run() {
    std::vector<int> in = {1, 2};
    printf("%d\n", append(in, nullptr));
    return 0;
}
}

namespace p_rawstring {
constexpr char kHeader[] = R"(#version 300 es
precision highp float;
"quoted" \n)";
constexpr char kDelimited[] = R"x(a)"b)x";
int run() {
    printf("%zu %c %d %s\n", strlen(kHeader), kHeader[1], (int)(strchr(kHeader, '\\') != nullptr), kDelimited);
    return 0;
}
}

namespace p_unionarray {
struct Layout {
    Layout() = default;
    int location;
};
struct Public {
    Public() = default;
    Layout layout;
    int qualifier;
};
union Value {
    struct {
        union {
            const char *string;
            int i;
        };
        const void *symbol;
    } lex;
    struct {
        int op;
        union {
            Layout layout;
            Public type;
        };
    } interm;
};
int run(int argc) {
    Value stack[4];
    Public publics[2];
    stack[1].lex.i = argc;
    publics[1].qualifier = 5;
    printf("%d %d\n", stack[1].lex.i, publics[1].qualifier);
    return 0;
}
}

namespace p_basetwice {
struct NonCopyable { int tag = 0; };
struct Node : NonCopyable { int id = 1; };
struct Traverser : NonCopyable { int depth = 2; };
struct Visitor : Node, Traverser {
    int found = 3;
};
struct Name {
    explicit Name(const Node &n) : value(n.id) {}
    bool operator==(const Name &o) const { return value == o.value; }
    int value;
};
int run() {
    Visitor v;
    Name n(v);
    printf("%d %d\n", (int)(Name(v) == n), v.found + v.depth);
    return 0;
}
}

int main(int argc, char **argv) {
    p_adl::run();
    p_newlist::run();
    p_constexprlocal::run();
    p_varargs::run();
    p_bracedcopy::run();
    p_elementlist::run();
    p_conversionref::run();
    p_nullptrparam::run();
    p_rawstring::run();
    p_unionarray::run(argc);
    p_basetwice::run();
    return 0;
}
