// A global's aggregate initializer holding a constexpr call's pointer
// into another global (`values.data()` of a std::array: JSC's HashTable
// of a generated binding, JSDOMWindowTable) gets a relocation to it, not
// the constant evaluator's own tagged address.
#include <array>
#include <cstdio>
struct V { const char* key; int n; };
struct T { int count; const V* values; const int* index; };
static const std::array<V, 3> values { { { "a", 1 }, { "b", 2 }, { "c", 3 } } };
static const int indexes[3] = { 0, 1, 2 };
static const T table = { 3, values.data(), indexes };
static constexpr std::array<V, 2> cvalues { { { "x", 1 }, { "y", 2 } } };
static const T ctable = { 2, cvalues.data(), indexes };
int main()
{
    std::printf("%d %d %s %s\n", table.values == values.data(), ctable.values == cvalues.data(), table.values[2].key, ctable.values[1].key);
    return 0;
}
