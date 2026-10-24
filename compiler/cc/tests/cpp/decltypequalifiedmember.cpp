// decltype of a qualified member whose qualifier has template arguments
// (`decltype(std::remove_pointer<E>::type::value)`) is the member's
// declared type, not a reference: ANGLE's PackParam (PackedEnums.h)
// asserts it is the type of the pointer it converts.
#include <stdio.h>
#include <type_traits>

struct ID { unsigned value; };
template <typename T> struct Box { using type = T; };

template <typename E, typename F>
E pack(F from) {
    static_assert(std::is_same<decltype(std::remove_pointer<E>::type::value),
                               typename std::remove_const<typename std::remove_pointer<F>::type>::type>::value,
                  "Data types are different");
    return reinterpret_cast<E>(from);
}

int main() {
    unsigned x = 5;
    ID *p = pack<ID *>(&x);
    const ID *q = pack<const ID *>(static_cast<const unsigned *>(&x));
    static_assert(std::is_same<decltype(Box<ID>::type::value), unsigned>::value, "Box");
    static_assert(std::is_same<decltype(Box<Box<ID>>::type::type::value), unsigned>::value, "nested");
    // A comparison is still an expression: decltype(a < b) is bool.
    int a = 1, b = 2;
    static_assert(std::is_same<decltype(a < b), bool>::value, "comparison");
    printf("%u %u\n", p->value, q->value);
    return 0;
}
