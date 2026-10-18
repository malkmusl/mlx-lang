// Forms of Skia's: std::min of a pointer and a class converting to one (its
// non-const conversion on a non-const object)
// plus an offset (`std::min(fTop, fVertexStack + 1)`); partial
// specializations of a member class template defined outside the class
// (`struct VertexWriter::is_quad<VertexWriter::TriFan<T>>`); a concept
// requiring `typename decltype(e)::element_type`.
#include <algorithm>
#include <concepts>
#include <cstdio>
#include <memory>
#include <type_traits>

template <int N, typename T> class AutoSTMalloc {
public:
    operator T *() { return fStorage; }
    operator const T *() const { return fStorage; }
    T &operator[](int i) { return fStorage[i]; }
private:
    T fStorage[N];
};

struct Vertex { int x; };

struct Triangulator {
    AutoSTMalloc<4, Vertex> fVertexStack;
    Vertex *fTop;
    int finish() {
        fTop = fVertexStack;
        fTop += 2;
        Vertex *end = std::min(fTop, fVertexStack + 1);
        return (int)(end - (Vertex *)fVertexStack);
    }
};

struct Writer {
    template <typename T> struct TriStrip { T l, t, r, b; };
    template <typename T> struct TriFan { T l, t, r, b; };
    template <typename T> struct is_quad : std::false_type {};
};
template <typename T> struct Writer::is_quad<Writer::TriStrip<T>> : std::true_type {};
template <typename T> struct Writer::is_quad<Writer::TriFan<T>> : std::true_type {};

struct StrikeBase { virtual ~StrikeBase() {} };
struct Strike : StrikeBase { int id = 3; };
template <typename T, template <typename...> class Template> constexpr bool is_specialization_of_v = false;
template <template <typename...> class Template, typename... Args> constexpr bool is_specialization_of_v<Template<Args...>, Template> = true;

template <typename T>
concept BackendData = requires(T &t, int cache) {
    T::FindStrike(cache);
    requires is_specialization_of_v<decltype(T::FindStrike(cache)), std::shared_ptr>;
    requires std::derived_from<typename decltype(T::FindStrike(cache))::element_type, StrikeBase>;
};

struct Data {
    static std::shared_ptr<Strike> FindStrike(int) { return std::make_shared<Strike>(); }
};
struct Other {
    static int FindStrike(int) { return 0; }
};

int main(int argc, char **argv) {
    Triangulator t;
    printf("%d %d %d %d\n", t.finish(), Writer::is_quad<Writer::TriFan<float>>::value, Writer::is_quad<Writer::TriStrip<int>>::value, Writer::is_quad<float>::value);
    printf("%d %d\n", BackendData<Data>, BackendData<Other>);
    return argc;
}
