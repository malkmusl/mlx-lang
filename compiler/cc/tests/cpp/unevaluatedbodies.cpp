// Functions named only where nothing is evaluated (a requires
// expression's `a == b` through a conversion operator, a function
// template's instance there) have no bodies made: Timer stays
// incomplete, and get()'s static_assert is never read.
#include <cstdio>
#include <concepts>

template<typename T> struct Weak {
    T* p { nullptr };
    T* get() const { static_assert(sizeof(T) > 0, "T must be complete"); return p; }
    operator T*() const { return get(); }
};

template<typename T> bool same(const Weak<T>& a, const Weak<T>& b) { return a.get() == b.get(); }

template<typename T> concept Comparable = requires(const T& a, const T& b) {
    { a == b };
};
template<typename T> concept Sameable = requires(const T& a) {
    same(a, a);
};

struct Timer;

int main(int argc, char** argv)
{
    std::printf("%d %d %d\n", (int)Comparable<Weak<Timer>>, (int)Sameable<Weak<Timer>>, (int)Comparable<int>);
    return 0;
}
