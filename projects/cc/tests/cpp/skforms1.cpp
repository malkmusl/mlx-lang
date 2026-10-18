// Forms of Skia's: std::size of an array passed by a template's
// reference and of a local static array sized by its initializer;
// `template <> [[maybe_unused]] inline` explicit specializations; a
// static member variable template expanded over a pack.
#include <cstdio>
#include <cstddef>
#include <iterator>
#include <tuple>

struct Parser {
    template <typename T, typename TArray> bool parseEnumMap(const TArray &arr, const char *name, T *result) {
        for (size_t i = 0; i < std::size(arr); ++i) {
            if (std::get<0>(arr[i])[0] == name[0]) {
                *result = std::get<1>(arr[i]);
                return true;
            }
        }
        return false;
    }
};

enum class Cap { kButt, kRound, kSquare };

static float seed(int i) {
    static constexpr float iota[] = {
        0.5f, 1.5f, 2.5f, 3.5f, 4.5f, 5.5f, 6.5f, 7.5f,
        8.5f, 9.5f, 10.5f, 11.5f, 12.5f, 13.5f, 14.5f, 15.5f,
    };
    static_assert(std::size(iota) >= 16);
    return iota[i];
}

struct Writer { int total = 0; };
struct Color { int fColor[2]; bool fWide; };
template <typename T> Writer &operator<<(Writer &w, const T &value) {
    w.total += (int)value;
    return w;
}
template <>
[[maybe_unused]] inline Writer &operator<<(Writer &w, const Color &color) {
    w << color.fColor[0];
    if (color.fWide) w << color.fColor[1];
    return w;
}

template <typename... Ts> class Zip {
    template <typename T> inline static constexpr T *nullify = nullptr;
public:
    constexpr Zip() : fPointers(nullify<Ts>...), fSize(0) {}
    Zip(size_t size, Ts *... ts) : fPointers(ts...), fSize(size) {}
    size_t size() const { return fSize; }
    bool empty() const { return std::get<0>(fPointers) == nullptr; }
private:
    std::tuple<Ts *...> fPointers;
    size_t fSize;
};

int main(int argc, char **argv) {
    static constexpr std::tuple<const char *, Cap> caps[] = {
        {"butt", Cap::kButt}, {"round", Cap::kRound}, {"square", Cap::kSquare},
    };
    Parser p;
    Cap cap = Cap::kButt;
    bool found = p.parseEnumMap(caps, "square", &cap);
    Writer w;
    w << Color{{3, 4}, true} << 10;
    Zip<int, float> empty;
    int is[2] = {1, 2};
    float fs[2] = {3, 4};
    Zip<int, float> zip(2, is, fs);
    printf("%d %d %.1f %d %d %zu\n", found, (int)cap, seed(3), w.total, empty.empty(), zip.size());
    return argc;
}
