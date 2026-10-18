// Pack patterns expanded in template arguments of a function's
// parameters (`const std::tuple<Ts *...> &pointers`, as Skia's SkZip),
// beside a parameter pack (`Ts *... ts`).
#include <cstdio>
#include <cstddef>
#include <tuple>

template <typename... Ts> class Zip {
public:
    constexpr Zip() : fPointers(), fSize(0) {}
    constexpr Zip(size_t size, Ts *... ts) : fPointers(ts...), fSize(size) {}
    constexpr size_t size() const { return fSize; }
    constexpr std::tuple<Ts *...> data() const { return fPointers; }
    constexpr Zip first(size_t n) const {
        if (n == 0) { return Zip(); }
        return Zip(n, fPointers);
    }
    void set(std::tuple<Ts *...> p, size_t n) {
        fPointers = p;
        fSize = n;
    }
private:
    constexpr Zip(size_t n, const std::tuple<Ts *...> &pointers) : fPointers(pointers), fSize(n) {}
    std::tuple<Ts *...> fPointers;
    size_t fSize;
};


int main(int argc, char **argv) {
    int is[3] = {1, 2, 3};
    float fs[3] = {4, 5, 6};
    Zip<int, float> zip(3, is, fs);
    Zip<int, float> two = zip.first(2);
    Zip<int, float> other;
    other.set(std::tuple<int *, float *>(is + 1, fs + 2), 1);
    printf("%zu %d %zu %.0f\n", two.size(), *std::get<0>(two.data()), other.size(), *std::get<1>(other.data()));
    return argc;
}
