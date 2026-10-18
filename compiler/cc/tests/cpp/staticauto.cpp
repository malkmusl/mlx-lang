// A function's `static auto` (and `static constexpr auto`) object: the
// initializer that gives its type is its value too, made the first time
// the declaration runs (WTF::toArray tables, as JSC's errorTypeName and
// Intl's roundingIncrement candidates have them).
#include <array>
#include <cstdio>
template<typename T, std::size_t N>
constexpr std::array<T, N> toArray(T (&&values)[N]) { return std::to_array<T>(static_cast<T(&&)[N]>(values)); }
struct Pair { int first, second; };
constexpr Pair makePair() { return { 3, 4 }; }
static int calls = 0;
int counted() { calls += 1; return 42; }
const char* name(unsigned index) {
    static constexpr auto names = toArray<const char*>({ "Error", "EvalError", "RangeError", "TypeError" });
    return names[index];
}
int main() {
    static constexpr auto candidates = toArray<unsigned>({ 1, 2, 5, 10, 5000 });
    static auto pair = makePair();
    for (int i = 0; i < 2; i++) { static auto once = counted(); printf("%d ", once); }
    printf("\n%zu %u %u %d %d %s %s %d\n", candidates.size(), candidates[0], candidates[4], pair.first, pair.second, name(3), name(2), calls);
    return 0;
}
