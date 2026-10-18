// Type traits of a class template's instance named only so far (a
// parameter's type, not yet needed whole): the instance is made first
// (Skia's SkNoDestructor<THashMap<...>> static_assert).
#include <cstdio>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>

template <typename T> class NoDestructor {
public:
    template <typename... Args> explicit NoDestructor(Args &&... args) { new (fStorage) T(std::forward<Args>(args)...); }
    T *get() { return reinterpret_cast<T *>(fStorage); }
    static_assert(!std::is_trivially_destructible_v<T>, "use a plain static");
private:
    alignas(T) unsigned char fStorage[sizeof(T)];
};

template <typename T> class Table {
public:
    Table() : fSlots(new T[4]), fCount(0) {}
    ~Table() = default;
    void add(T v) { fSlots[fCount++] = v; }
    int count() const { return fCount; }
private:
    std::unique_ptr<T[]> fSlots;
    int fCount;
};

template <typename K, typename V> class Map {
public:
    void set(K k) { fTable.add(k); }
    int count() const { return fTable.count(); }
private:
    Table<K> fTable;
};

static int size(const Map<int, char> *map);

static int fill() {
    static NoDestructor<Map<int, char>> tokens;
    tokens.get()->set(1);
    tokens.get()->set(2);
    return size(tokens.get());
}

static int size(const Map<int, char> *map) { return map->count(); }

int main(int argc, char **argv) {
    printf("%d %d\n", fill(), (int)std::is_trivially_destructible_v<Map<long, int>>);
    return argc;
}
