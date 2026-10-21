// A generic lambda's body naming the enumerators a `using enum` of the
// function around brought in (`mappedDisplayType<outside, Flow>()` in a
// `[]<DisplayOutside outside>(...)` of WebCore's StyleDisplay.cpp:
// "undeclared identifier 'Flow'"), its operator() read for an instance
// once that function is put aside; a local enum's own too.
#include <stdio.h>

enum class Outside { Block, Inline };
enum class Inside { Flow, FlowRoot, Table };

template<Outside outside, Inside inside> constexpr int mapped() { return (int)outside * 10 + (int)inside; }

int display(Inside inside)
{
    using enum Inside;
    enum Local { First = 7, Second };
    auto handle = []<Outside outside>(Inside value) {
        switch (value) {
        case Flow: return mapped<outside, Flow>();
        case FlowRoot: return mapped<outside, FlowRoot>() + Second;
        default: return mapped<outside, Table>() + First;
        }
    };
    return handle.template operator()<Outside::Inline>(inside);
}

int main(int argc, char **argv)
{
    printf("%d %d %d\n", display(Inside::Flow), display(Inside::FlowRoot), display(Inside::Table));
    return argc;
}
