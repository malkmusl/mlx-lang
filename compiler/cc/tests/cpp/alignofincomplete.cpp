// alignof of a class template's instance named only over a class not
// defined yet does not make the instance: made then it would lay out
// nothing of that class. JSC's JSONRanges declares `struct Entry;`, a
// HashMap of Entry, then Entry holding a Variant of that map: the
// HashTable's `alignof(ValueType)` made KeyValuePair<K, Entry> 8 bytes
// and its buckets were walked 8 bytes apart.
#include <new>
#include <stdio.h>
#include <stdlib.h>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

template<class K, class V> struct Pair { K key; V value; };
template<class T> struct Table {
    static constexpr unsigned metadata = alignof(T) > 16 ? alignof(T) : 16;
    T *table = nullptr;
    unsigned count = 0;
    Table() = default;
    Table(Table &&other) : table(other.table), count(other.count) { other.table = nullptr; other.count = 0; }
    Table &operator=(Table &&other) { std::swap(table, other.table); std::swap(count, other.count); return *this; }
    void add(T &&value)
    {
        T *grown = static_cast<T *>(malloc(sizeof(T) * (count + 1)));
        for (unsigned i = 0; i < count; ++i) {
            new (&grown[i]) T(std::move(table[i]));
            table[i].~T();
        }
        new (&grown[count]) T(std::move(value));
        free(table);
        table = grown;
        ++count;
    }
    ~Table()
    {
        for (unsigned i = 0; i < count; ++i) table[i].~T();
        free(table);
    }
};
template<class K, class V> struct Map {
    Table<Pair<K, V>> impl;
    void add(K k, V &&v) { impl.add(Pair<K, V> { k, std::move(v) }); }
};

struct Ranges {
    struct Entry;
    using Object = Map<int, Entry>;
    struct Entry {
        long value { 0 };
        std::variant<std::monostate, Object, std::vector<long>> properties;
    };
};

int main(int argc, char **)
{
    Ranges::Object object;
    for (int i = 0; i < 20; ++i) {
        Ranges::Entry e;
        e.value = i;
        e.properties = std::vector<long> { i * 2L };
        object.add(i, std::move(e));
    }
    long sum = 0;
    for (unsigned i = 0; i < object.impl.count; ++i) sum += object.impl.table[i].value.value + std::get<2>(object.impl.table[i].value.properties)[0];
    printf("%d %ld %u\n", (int)(sizeof(Pair<int, Ranges::Entry>) > 8), sum, object.impl.count);
    return argc;
}
