// A lambda in a lambda that captures `this` holds the class's `this`,
// not the outer closure's address: the inner closure kept and called
// after the outer one is gone still reaches the object
// (JavaScriptCore's WasmOMGIRGenerator::emitDirectCall: `[&, this](...) {
// patchpoint->setGenerator([this, ...](...) { ... }); }`, the generator
// run when B3 generates code). Three levels, explicit and implicit
// captures, a generic lambda inside.
#include <functional>
#include <stdio.h>

struct Generator {
    int value = 40;
    Generator* root = this;
    std::function<int()> first;
    std::function<int()> second;
    std::function<int()> third;
    std::function<int(int)> fourth;

    void emit(int k)
    {
        auto outer = [&, this](int extra) {
            first = [this, k, extra]() { return root->value + k + extra + (root == this ? 1000 : 0); };
            auto middle = [=, this]() {
                second = [=, this]() { return value * 2 + k; };
                third = [this] { return [this] { return value + 3; }(); };
            };
            middle();
            fourth = [this](auto x) { return value + x; };
        };
        outer(1);
    }
};

int main()
{
    Generator generator;
    generator.emit(5);
    int junk[64];
    for (int& j : junk)
        j = 7;
    printf("%d %d %d %d %d\n", generator.first(), generator.second(), generator.third(), generator.fourth(2), junk[3]);
    return 0;
}
