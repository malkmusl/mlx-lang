// A switch whose condition declares its variable (WebCore's RenderGrid:
// `switch (const auto alignSelf = selfAlignment(...).position())`): the
// variable in scope through the cases, destroyed after the switch.
#include <stdio.h>
enum class Position { Start, End, Center };
Position position(int x) { return (Position)(x % 3); }
struct Code {
    int value;
    operator int() const { return value; }
    ~Code() { printf("~Code %d\n", value); }
};

int classify(int x)
{
    switch (const auto p = position(x)) {
    case Position::Start:
        return 10;
    case Position::End:
        return p == Position::End ? 20 : -1;
    default:
        return (int)p * 100;
    }
}

int coded(int x)
{
    int result = 0;
    switch (Code c { x * 2 }) {
    case 4:
        result = 1;
        break;
    default:
        result = c.value;
    }
    printf("after switch\n");
    return result;
}

int main(int argc, char **)
{
    printf("%d %d %d\n", classify(0), classify(1), classify(2));
    printf("%d\n", coded(2));
    printf("%d\n", coded(5));
    return argc;
}
