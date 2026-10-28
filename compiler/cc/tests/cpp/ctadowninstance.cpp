// Class template argument deduction from an object of the template's own:
// the copy deduction candidate, not a constructor's guide that would
// instantiate Set<Set<Flag>> (JSC's `OptionSet seenModes = ...`).
#include <stdio.h>
#include <type_traits>
enum class Tag : bool { A, B };
template<typename E, Tag t = Tag::A> class Set {
    static_assert(std::is_enum<E>::value, "E is not an enum type");
public:
    constexpr Set() = default;
    constexpr Set(E e) : m_bits(static_cast<unsigned>(e)) { }
    unsigned m_bits { 0 };
};
enum class Flag : unsigned { X = 1, Y = 2 };
struct Node { Set<Flag> metadata() const { return Set<Flag>(Flag::Y); } };
int main() { Node n; Set seen = n.metadata(); Set other = Flag::X; Set copy(seen); printf("%u %u %u\n", seen.m_bits, other.m_bits, copy.m_bits); return 0; }
