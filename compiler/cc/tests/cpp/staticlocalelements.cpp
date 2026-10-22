// A static local of a function whose initializer holds class elements
// made by expressions (`static constexpr std::array pairs { std::array {
// ... }, ... }` in WebCore's TransformOperation, the elements deduced):
// assigned the first time its declaration runs, with the function's
// temporaries. Made when the program started, the temporaries had no
// place in that frame and were written over its saved registers.
#include <array>
#include <cstdio>
#include <optional>
enum class Type : unsigned char { A = 8, A3 = 18, B = 3, B3 = 4, C = 5, C3 = 6 };
struct Op {
    Type t;
    std::optional<Type> shared(Type other) const
    {
        if (t == other)
            return t;
        static constexpr std::array pairs {
            std::array { Type::A, Type::A3 },
            std::array { Type::B, Type::B3 },
            std::array { Type::C, Type::C3 }
        };
        for (auto pair : pairs) {
            if ((t == pair[0] || t == pair[1]) && (other == pair[0] || other == pair[1]))
                return pair[1];
        }
        return std::nullopt;
    }
};
int main()
{
    Op op { Type::A };
    auto r = op.shared(Type::A3);
    std::printf("%d %d\n", r ? (int)*r : -1, op.shared(Type::B) ? 1 : 0);
    return 0;
}
