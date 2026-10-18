#include "units.h"

int Queue::fromB()
{
    const char* text = "abc";
    long scale = 2;
    WrapperBase* w = wrap([text, scale] { return text[1] * (int)scale; });
    int result = w->call();
    delete w;
    return result;
}
