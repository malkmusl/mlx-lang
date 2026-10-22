// A lambda capturing nothing names an outer variable in decltype, an
// unevaluated operand (RuleFeature's `makeUnique<std::decay_t<decltype(
// *keyValuePair.value)>>()` inside `[] { }`).
#include <stdio.h>
#include <memory>
#include <type_traits>
#include <vector>
struct Pair { int key; std::vector<int>* value; };
int main(int argc, char** argv)
{
    std::vector<int> numbers { 1, 2 };
    Pair keyValuePair { 1, &numbers };
    auto make = [] { return std::make_unique<std::decay_t<decltype(*keyValuePair.value)>>(); };
    auto made = make();
    made->push_back(5);
    printf("%zu %d\n", made->size(), (int) sizeof(decltype(keyValuePair.key)));
    return argc - 1;
}
