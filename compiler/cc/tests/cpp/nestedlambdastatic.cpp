// A lambda in a lambda naming a static local of the function around
// both (no capture: WebCore's AsyncFileStream callOnFileThread).
#include <cstdio>

template<typename F> void run(F f) { f(); }

static int counter(int add)
{
    static int total = 0;
    run([add] {
        run([add] {
            total += add;
        });
        total += 1;
    });
    return total;
}

int main(int argc, char** argv)
{
    counter(argc);
    std::printf("%d\n", counter(10));
    return 0;
}
