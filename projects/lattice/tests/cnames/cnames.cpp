// The translator's test of: see tools/check_lattice_emit.sh (cnames).
#include <sys/stat.h>
#include <cmath>
#include <signal.h>
namespace cnames {
unsigned run()
{
    struct stat info;
    info.st_size = 0;
    unsigned total = 0;
    if (stat("/definitely/not/here", &info) != 0) total += 7;
    total += (unsigned)info.st_size;
    float f = std::copysign(3.0f, -1.0f);
    double d = copysign(2.0, -0.5);
    total += (unsigned)(int)(f * -10.0f) + (unsigned)(int)(d * -100.0);
    struct sigaction action;
    action.sa_flags = 5;
    total += (unsigned)action.sa_flags;
    return total;
}
}
