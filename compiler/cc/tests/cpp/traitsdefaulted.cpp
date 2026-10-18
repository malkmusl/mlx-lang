// Type traits of classes with defaulted special members: a defaulted
// destructor is trivial only when the members' and bases' are (Skia's
// THashTable, `~THashTable() = default;` over a unique_ptr); a class
// whose copy constructor is deleted (privately) is not copy
// constructible (Skia's SkMessageBus messages).
#include <cstdio>
#include <memory>
#include <type_traits>

template <typename T> class Table {
public:
    Table() = default;
    ~Table() = default;
private:
    std::unique_ptr<T[]> fSlots;
    int fCount = 0;
};

template <typename K, typename V> class Map {
    Table<V> fTable;
};

struct Pod {
    ~Pod() = default;
    int x;
};

struct Inherits : Table<int> {
    ~Inherits() = default;
};

class Message {
public:
    Message(Message &&) = default;
    Message &operator=(Message &&) = default;
private:
    Message(int v) : fValue(v) {}
    Message(const Message &) = delete;
    Message &operator=(const Message &) = delete;
    std::unique_ptr<int> fResource;
    int fValue;
};

struct NoCopy {
    NoCopy() {}
    NoCopy(const NoCopy &) = delete;
};

struct Copyable {
    Copyable(Copyable &&) = default;
    Copyable(const Copyable &) = default;
    int x;
};

int main(int argc, char **argv) {
    printf("%d %d %d %d\n", std::is_trivially_destructible_v<Table<int>>, std::is_trivially_destructible_v<Map<int, float>>,
           std::is_trivially_destructible_v<Pod>, std::is_trivially_destructible_v<Inherits>);
    printf("%d %d %d %d\n", std::is_copy_constructible<Message>::value, std::is_move_constructible<Message>::value,
           std::is_copy_constructible<Copyable>::value, std::is_trivially_constructible_v<Map<int, int>>);
    printf("%d %d\n", std::is_copy_constructible<NoCopy>::value, std::is_move_constructible<NoCopy>::value);
    return argc;
}
