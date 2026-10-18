// A local of the function around a lambda hides a global name the same
// way in the lambda's body: here a static local `id` behind the global
// typedef `id` (as WTF's Forward.h declares one), read in a lambda that a
// template calls.
#include <cstdio>

typedef struct objc_object* id;

struct Holder {
    int value;
    int& get() { return value; }
};

template<typename F> void callIt(F f) { f(); }

static int counter()
{
    static Holder id;
    callIt([] {
        id.get() += 3;
    });
    return id.get();
}

int main()
{
    counter();
    std::printf("%d\n", counter());
    id pointer = nullptr;
    std::printf("%d\n", pointer == nullptr);
    return 0;
}
