// A base initialized with braces in a constructor's initializers, the
// base having an initializer_list constructor: the list when the
// elements fit it, another constructor otherwise (Skia's STArray:
// `TArray<T>{array, count, this}`).
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <vector>

template <int N, typename T>
struct Storage {
    alignas(T) char bytes[N * sizeof(T)];
    void *get() { return bytes; }
};

template <typename T>
class Array {
public:
    Array() : fData(nullptr), fSize(0) {}
    Array(const T *array, int count) : fData(new T[count]), fSize(count) { memcpy(fData, array, count * sizeof(T)); }
    Array(std::initializer_list<T> data) : Array(data.begin(), (int)data.size()) {}
    int size() const { return fSize; }
    T operator[](int i) const { return fData[i]; }
protected:
    template <int InitialCapacity>
    Array(Storage<InitialCapacity, T> *storage, int size = 0) : fData((T *)storage->get()), fSize(size) {}
    template <int InitialCapacity>
    Array(const T *array, int size, Storage<InitialCapacity, T> *storage) : Array{storage, size} {
        memcpy(fData, array, size * sizeof(T));
    }
    T *fData;
    int fSize;
};

template <int N, typename T>
class SArray : private Storage<N, T>, public Array<T> {
public:
    SArray() : Storage<N, T>{}, Array<T>(this) {}
    SArray(const T *array, int count) : Storage<N, T>{}, Array<T>{array, count, this} {}
    SArray(std::initializer_list<T> data) : SArray{data.begin(), (int)data.size()} {}
    SArray(const SArray &) = delete;
};

struct Numbers : std::vector<int> {
    Numbers(int a, int b) : std::vector<int>{a, b, a + b} {}
};

struct Type { int id; };

int main(int argc, char **argv) {
    Type x{4}, y{5};
    SArray<2, const Type *> types{&x, &y};
    SArray<4, int> numbers{7, 8, 9};
    Numbers sums(2, 3);
    printf("%d %d %d %d %d\n", types.size(), types[0]->id, types[1]->id, numbers.size(), numbers[2]);
    printf("%zu %d\n", sums.size(), sums[2]);
    return argc;
}
