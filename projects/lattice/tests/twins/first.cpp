#include "shared.h"
// A record of the unit's own with a name the other unit uses for another.
struct Guts { unsigned count; unsigned long long big; twins::Shared shared; };
static unsigned weigh(const Guts &guts) { return guts.count + (unsigned)(guts.big % 97) + guts.shared.a; }
namespace twins {
unsigned run()
{
    Guts guts;
    guts.count = 5;
    guts.big = 1000003;
    guts.shared.a = 11;
    guts.shared.b = 13;
    return weigh(guts) * 100 + second(guts.shared);
}
}
