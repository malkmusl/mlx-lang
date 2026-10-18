// WTF/JSC forms 29: auto template parameters typed by their arguments
#include <stdio.h>
#include <type_traits>

namespace p_autonttp {
enum class Kind { A, B, C };
template<auto V> constexpr int nameLen() { static_assert(std::is_enum_v<decltype(V)>, "enum"); return (int)V + 10; }
template<typename E, auto V> constexpr int named() { return nameLen<static_cast<E>(V)>(); }
template<auto V> int sizeOf() { return (int)sizeof(V); }
int run() { printf("%d %d %d %d\n", nameLen<Kind::C>(), named<Kind, 1>(), sizeOf<'c'>(), sizeOf<5L>()); return 0; }
}

int main() {
    p_autonttp::run();
    return 0;
}
