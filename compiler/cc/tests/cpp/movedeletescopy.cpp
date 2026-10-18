// A class declaring a move constructor or move assignment has no copy
// constructor and no copy assignment (they are defined as deleted):
// the type traits say so (Skia's TClientMappedBufferManager message,
// checked by SkMessageBus).
#include <cstdio>
#include <type_traits>
#include <utility>

struct MoveOnly {
    MoveOnly(int v) : value(v) {}
    MoveOnly(MoveOnly &&other) : value(other.value) { other.value = 0; }
    int value;
};

struct MoveAssigned {
    MoveAssigned() {}
    MoveAssigned &operator=(MoveAssigned &&) { return *this; }
};

struct Plain {
    int value;
};

template <typename T>
struct Outer {
    struct Message {
        Message(T v) : value(v) {}
        Message(Message &&other) { value = other.value; }
        T value;
    };
};

int main(int argc, char **argv) {
    printf("%d %d %d\n", std::is_copy_constructible<MoveOnly>::value, std::is_move_constructible<MoveOnly>::value, std::is_copy_assignable<MoveOnly>::value);
    printf("%d %d %d\n", std::is_copy_constructible_v<MoveAssigned>, std::is_copy_assignable_v<MoveAssigned>, std::is_move_assignable_v<MoveAssigned>);
    printf("%d %d\n", std::is_copy_constructible_v<Plain>, std::is_copy_constructible_v<Outer<int>::Message>);
    MoveOnly a(4);
    MoveOnly b(std::move(a));
    printf("%d %d\n", a.value, b.value);
    return argc;
}
