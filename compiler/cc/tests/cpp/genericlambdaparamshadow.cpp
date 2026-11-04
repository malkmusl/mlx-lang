// A generic lambda's parameter hides the function's local of its name:
// `[=]` does not capture that local (B3's CheckSpecial::generate: `[=,
// this] (auto& jit, GenerationContext& context) { ... }` in a function
// whose own `jit` is a CCallHelpers &: the closure copied the assembler,
// its buffer and all, and freed the buffer twice).
#include <stdio.h>

static int copies = 0;

struct Assembler {
    int code = 7;
    Assembler() = default;
    Assembler(const Assembler& other) : code(other.code) { ++copies; }
};

template<typename F>
static int run(F f)
{
    Assembler other;
    other.code = 100;
    return f(other, 5);
}

static int generate(Assembler& jit, int context)
{
    int base = 3;
    auto late = [=](auto& jit, int context) { return jit.code + context + base; };
    return run(late) + jit.code + context + (int)sizeof(late);
}

int main()
{
    Assembler jit;
    printf("%d %d\n", generate(jit, 1000), copies);
    return 0;
}
