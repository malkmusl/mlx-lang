// An inline static data member without an initializer is a definition
// (default-initialized; weak, one object for the program): WebCore's
// NavigatorUAData has `inline static LazyNeverDestroyed<Vector<...>>
// m_brands;`, which no file defined.
#include <cstdio>

template<typename T> struct Lazy {
    void construct(T v) { value = v; constructed = true; }
    T get() const { return constructed ? value : T(-1); }
    T value;
    bool constructed;
};

struct Data {
    inline static Lazy<int> lazy;
    static inline int counter;
    inline static int array[3];
    static int bump() { return ++counter; }
};

int main()
{
    std::printf("%d\n", Data::lazy.get());
    Data::lazy.construct(42);
    Data::array[2] = 7;
    Data::bump();
    std::printf("%d %d %d %d\n", Data::lazy.get(), Data::bump(), Data::array[0], Data::array[2]);
    return 0;
}
