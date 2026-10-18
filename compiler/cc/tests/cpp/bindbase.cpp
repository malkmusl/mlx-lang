// A structured binding of a class whose data members are its base's
// (Skia's `const auto& [field, info] = *pair;` of a THashMap::Pair, a
// std::pair's derived class with no tuple_size of its own).
#include <cstdio>
#include <utility>

template <typename K, typename V> struct Map {
    struct Pair : public std::pair<K, V> {
        using std::pair<K, V>::pair;
        static const K &GetKey(const Pair &p) { return p.first; }
    };
};

struct Base { int x; float y; };
struct Derived : Base {};

int main(int argc, char **argv) {
    Map<int, const char *>::Pair pair(3, "three");
    const Map<int, const char *>::Pair *p = &pair;
    const auto &[field, info] = *p;
    Derived d;
    d.x = 7;
    d.y = 1.5f;
    auto [x, y] = d;
    printf("%d %s %d %.1f\n", field, info, x, y);
    return argc;
}
