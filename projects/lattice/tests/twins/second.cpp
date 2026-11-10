#include "shared.h"
struct Guts { const char *text; unsigned char flag; };
static unsigned weigh(const Guts &guts) { return (unsigned)guts.text[1] + guts.flag; }
namespace twins {
unsigned second(const Shared &shared)
{
    Guts guts;
    guts.text = "xyz";
    guts.flag = 7;
    return weigh(guts) + shared.b;
}
}
