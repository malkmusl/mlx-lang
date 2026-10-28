// An `auto...` template parameter pack's values keep their arguments'
// types (an enum's value calls the enum's overload, not long's) and
// classes (a class's value is that object), after a type parameter too:
// JavaScriptCore's allocatedScratchRegisters<GPRInfo, valueJSR, protoJSR,
// propertyCacheGPR, GPRInfo::handlerGPR> of JSValueRegs and GPRReg values
// (BaselineJITRegisters.h).
#include <array>
#include <stdio.h>

typedef enum : signed char { r0, r1, r2, r3, r4, r5, Invalid = -1 } Reg;

struct Regs {
    constexpr explicit Regs(Reg g) : m_gpr(g) { }
    constexpr Reg payloadGPR() const { return m_gpr; }
    Reg m_gpr;
};

constexpr int kind(Reg) { return 1; }
constexpr int kind(long) { return 2; }
constexpr int kind(Regs) { return 3; }
constexpr Reg reg(Regs r) { return r.payloadGPR(); }
constexpr Reg reg(Reg r) { return r; }

template<auto R> struct One { static constexpr int f() { return kind(R); } };
template<auto... R> struct Many { static constexpr int f() { return ((kind(R) * 10) + ... + 0); } };
template<typename T, auto... R> struct After { static constexpr int f() { return ((kind(R) * 10) + ... + 0); } };

struct Bank {
    static constexpr unsigned numberOfRegisters = 6;
    static constexpr Reg toRegister(unsigned i) { return static_cast<Reg>(i); }
};

template<typename B, auto... registers>
struct Scratch {
    static constexpr bool used(Reg candidate) { return ((reg(registers) == candidate) || ...); }
    static constexpr std::array<Reg, 3> make()
    {
        std::array<Reg, 3> array { };
        unsigned at = 0;
        for (unsigned i = 0; i < B::numberOfRegisters && at < 3; ++i) {
            if (!used(B::toRegister(i)))
                array[at++] = B::toRegister(i);
        }
        return array;
    }
    static constexpr std::array<Reg, 3> values { make() };
};

template<typename B, auto... registers>
inline constexpr auto scratchRegisters = Scratch<B, registers...>::values;

static constexpr Reg c = r3;
static constexpr Regs value { r1 };
static constexpr auto scratch = scratchRegisters<Bank, value, c, r0>;
static constexpr Reg first { scratch[0] };
static_assert(first == r2 && scratch[1] == r4, "the scratch registers, at compile time");

int main()
{
    printf("%d %d %d %d %d\n", One<c>::f(), Many<c>::f(), Many<c, 5L>::f(), After<int, c>::f(), Many<value>::f());
    printf("%d %d %d %d\n", first, scratch[0], scratch[1], scratch[2]);
    return 0;
}
