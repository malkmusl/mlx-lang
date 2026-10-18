// A nested lambda's explicit capture of an outer lambda's capture whose
// name is also a function's: `[&main]` of a local `main` in main() (as
// WTF's programs name the main run loop), by reference and by copy,
// under an implicit capture and an explicit one, through templates.
#include <stdio.h>

template<typename F> void call(F f) { f(); }

static int counter() { return 100; }

struct Loop {
    int ticks = 0;
    void tick(int by) { ticks += by; }
};

int main(int argc, char** argv)
{
    Loop loop;
    Loop& main = loop;
    call([&] {
        call([&main] {
            int step = 5;
            call([step, &main] { main.tick(step); });
            call([&main] { main.tick(1); });
        });
    });
    int counter = 7;
    call([&counter] {
        call([counter] { printf("copied %d\n", counter); });
        call([&counter] { counter += 3; });
    });
    printf("ticks %d counter %d function %d\n", loop.ticks, counter, ::counter());
    return argc - 1;
}
