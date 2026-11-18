// A braced list given to overloads that take vectors of different
// elements: the overload whose element the list's elements convert to is
// called, not the first whose vector a list could make (the conformance
// suite's SPIR-V assembler calls, vkSpirVProgram's operand lists).
#include <stdio.h>
#include <string>
#include <vector>

struct Immediate
{
    bool isId;
    unsigned word;
};

static int f(int, const std::vector<unsigned> &operands)
{
    return 1 + (int)operands.size();
}

static int f(int, const std::vector<Immediate> &operands)
{
    return 100 + (int)operands.size();
}

static int g(const std::vector<std::string> &names)
{
    return 10 + (int)names.size();
}

static int g(const std::vector<int> &numbers)
{
    return 20 + (int)numbers.size();
}

int main()
{
    Immediate i = {false, 7};
    unsigned u  = 3;
    printf("%d %d %d\n", f(0, {i}), f(0, {u, u}), f(0, {i, i, i}));
    printf("%d %d\n", g({"a", "b", "c"}), g({1, 2, 3}));
    return 0;
}
