// A using-declaration naming a base's overloaded members (`using
// TArray<T>::data;`) hides the members of that name of the other bases:
// a call takes the named base's overloads, the const one for a const
// object only.
#include <cstdio>
#include <cstddef>
#include <type_traits>

template <int N, typename T> class Storage {
public:
    unsigned char *data() { return fStorage; }
    const unsigned char *data() const { return fStorage; }
    size_t size() const { return sizeof(fStorage); }
protected:
    alignas(T) unsigned char fStorage[sizeof(T) * N];
};

template <typename T> class Array {
public:
    Array(T *p) : fData(p), fSize(0) {}
    void push(const T &t) { fData[fSize++] = t; }
    T *data() { return fData; }
    const T *data() const { return fData; }
    int size() const { return fSize; }
private:
    T *fData;
    int fSize;
};

template <int N, typename T>
class SArray : private Storage<N, T>, public Array<T> {
public:
    SArray() : Storage<N, T>{}, Array<T>((T *)this->fStorage) {}
    using Array<T>::data;
    using Array<T>::size;
};

struct Plain : private Storage<3, int>, public Array<int> {
    Plain() : Array<int>((int *)fStorage) {}
    using Array<int>::data;
    using Array<int>::size;
};

const char *kind(int *) { return "int *"; }
const char *kind(const int *) { return "const int *"; }
const char *kind(unsigned char *) { return "bytes"; }
const char *kind(const unsigned char *) { return "const bytes"; }

int main(int argc, char **argv) {
    SArray<4, int> a;
    a.push(7);
    const SArray<4, int> &ca = a;
    static_assert(std::is_same_v<decltype(a.data()), int *>);
    static_assert(std::is_same_v<decltype(ca.data()), const int *>);
    static_assert(std::is_same_v<decltype(a.size()), int>);
    Plain p;
    p.push(9);
    const Plain &cp = p;
    printf("%s %s %d %d\n", kind(a.data()), kind(ca.data()), a.size(), a.data()[0]);
    printf("%s %s %d %d\n", kind(p.data()), kind(cp.data()), p.size(), p.data()[0]);
    return argc;
}
