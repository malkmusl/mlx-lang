// std::is_member_object_pointer is true for a pointer to a data member
// (it was false for all), and it and is_member_function_pointer have
// their _v forms: WebCore's CSSPrimitiveValue static_asserts
// `is_member_object_pointer_v<decltype(&CSSPrimitiveValue::m_has...)>`.
#include <stdio.h>
#include <type_traits>

class Value {
public:
    int serialize();
    int twice(int x) const { return x * 2; }
    long count { 3 };

private:
    bool m_cached { false };
};

int Value::serialize()
{
    static_assert(std::is_member_object_pointer_v<decltype(&Value::m_cached)>, "must not be a bit-field");
    static_assert(std::is_same_v<decltype(&Value::m_cached), bool Value::*>);
    m_cached = true;
    return m_cached;
}

int main(int argc, char **)
{
    Value v;
    printf("%d %d %d %d\n", (int)std::is_member_object_pointer_v<long Value::*>, (int)std::is_member_object_pointer_v<int (Value::*)(int) const>, (int)std::is_member_function_pointer_v<decltype(&Value::twice)>, (int)std::is_member_function_pointer_v<long Value::*>);
    printf("%d %d %d %d\n", (int)std::is_member_object_pointer<const long Value::*const>::value, (int)std::is_member_object_pointer_v<long *>, (int)std::is_member_pointer_v<decltype(&Value::count)>, v.serialize());
    return argc;
}
