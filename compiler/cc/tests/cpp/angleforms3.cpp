// ANGLE forms 3: a namespace named where a parameter of its name hides
// it, `extern const` objects defined with braces, globals initialized
// with parentheses, braces as operator[]'s argument, a base's implicit
// operator= called by name, the static data members of a member class
// template's instance specialized, a free operator taking a variable
// beside a member operator template taking it as const.
#include <stdio.h>
#include <stddef.h>
#include <array>
#include <map>
#include <string>
#include <sstream>
#include <utility>

namespace p_hidden {
namespace angle {
struct Vector3 { float x, y, z; Vector3(float a, float b, float c) : x(a), y(b), z(c) {} };
struct Mat4 { float v; static Mat4 Rotate(float angle, const Vector3 &axis) { return Mat4{angle + axis.x + axis.y + axis.z}; } };
}
inline float rotate(float angle, float x, float y, float z) { return angle::Mat4::Rotate(angle, angle::Vector3(x, y, z)).v; }
int run() { printf("%g\n", rotate(1, 2, 3, 4)); return 0; }
}

namespace p_externtable {
struct NonCopyable {
    constexpr NonCopyable() = default;
    ~NonCopyable() = default;
    NonCopyable(const NonCopyable &) = delete;
    void operator=(const NonCopyable &) = delete;
};
enum class Format { Float, Int, InvalidEnum };
struct Info final : NonCopyable {
    inline constexpr Info(unsigned type, Format format, int rows, size_t size, bool sampler);
    unsigned type;
    Format format;
    int rows;
    size_t size;
    bool sampler;
};
inline constexpr Info::Info(unsigned type, Format format, int rows, size_t size, bool sampler)
    : type(type), format(format), rows(rows), size(size), sampler(sampler) {}
extern const std::array<Info, 3> kTable = {
    {{0, Format::InvalidEnum, 0, 0 * 0, false},
     {0x8b56, Format::Float, 1, 4 * 1, false},
     {0x1406, Format::Int, 4, sizeof(float) * 4, true}}};
struct Context;
Context *gLastContext(nullptr);
static long gValue(42);
int run() {
    printf("%x %d %d %zu %d %d %ld\n", kTable[1].type, (int)kTable[2].format, kTable[2].rows, kTable[2].size, (int)kTable[2].sampler, (int)(gLastContext == nullptr), gValue);
    return 0;
}
}

namespace p_bracedindex {
int run() {
    std::map<std::pair<int, int>, int> m;
    m[{1, 2}] = 5;
    m[{3, 4}] = 7;
    printf("%d %d %zu\n", m[{1, 2}], m[{3, 4}], m.size());
    return 0;
}
}

namespace p_baseassign {
struct Base { int a = 1; };
struct Binding : Base {
    int b = 2;
    Binding &operator=(const Binding &other) {
        Base::operator=(other);
        b = other.b * 10;
        return *this;
    }
};
int run() {
    Binding p, q;
    q.a = 5;
    q.b = 6;
    p = q;
    printf("%d %d\n", p.a, p.b);
    return 0;
}
}

namespace p_memberstatic {
class Name {
  public:
    template <size_t hashBytes> struct Hash {
        static const size_t kOffset;
        static const size_t kPrime;
        size_t operator()(const char *data) const {
            size_t hash = kOffset;
            while (*data != 0) {
                hash = hash ^ (unsigned char)*data;
                hash = hash * kPrime;
                ++data;
            }
            return hash;
        }
    };
};
template <> const size_t Name::Hash<8>::kPrime = 1099511628211ull;
template <> const size_t Name::Hash<8>::kOffset = 0xcbf29ce484222325ull;
int run() { printf("%zx\n", Name::Hash<8>()("abc")); return 0; }
}

namespace p_freeoperator {
class Sink {
  public:
    template <typename T> Sink &operator<<(const T &t) {
        std::ostringstream stream;
        stream << t;
        text += stream.str();
        return *this;
    }
    Sink &operator<<(char c) { text += c; return *this; }
    Sink &operator<<(const char *s) { text += s; return *this; }
    std::string text;
};
namespace {
class Prefix {
  public:
    Prefix() : first(true) {}
  private:
    bool first;
    template <typename Stream> friend Stream &operator<<(Stream &out, Prefix &gen);
};
template <typename Stream> Stream &operator<<(Stream &out, Prefix &gen) {
    if (gen.first) gen.first = false; else out << ", ";
    return out;
}
}
int run() {
    Sink sink;
    Prefix prefix;
    for (int i = 0; i < 3; i++) sink << prefix << i;
    sink << 'x' << "yz" << 2.5;
    printf("%s\n", sink.text.c_str());
    return 0;
}
}

int main() {
    p_hidden::run();
    p_externtable::run();
    p_bracedindex::run();
    p_baseassign::run();
    p_memberstatic::run();
    p_freeoperator::run();
    return 0;
}
