#include "units.h"

int Queue::fromA()
{
    int base = 10;
    WrapperBase* w = wrap([base] { return base + 1; });
    int result = w->call();
    delete w;
    return result;
}

int main()
{
    Queue queue;
    printf("a %d\n", queue.fromA());
    printf("b %d\n", queue.fromB());
    return 0;
}
