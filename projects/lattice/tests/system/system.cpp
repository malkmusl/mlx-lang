// The translator's test of: see tools/check_lattice_emit.sh (system).
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <linux/futex.h>
#include <inttypes.h>
namespace system {
unsigned run()
{
    unsigned total = 0;
    unsigned word = 5;
    // (The word is not 7: the wait returns at once.)
    long waited = syscall(SYS_futex, &word, FUTEX_WAIT | FUTEX_PRIVATE_FLAG, 7, nullptr, nullptr, 0);
    total += waited == -1 ? 11 : 0;
    long woken = syscall(SYS_futex, &word, FUTEX_WAKE | FUTEX_PRIVATE_FLAG, 1, nullptr, nullptr, 0);
    total += (unsigned)woken + 3;
    int copy = fcntl(1, F_DUPFD_CLOEXEC, 100);
    total += copy >= 100 ? 1000 : 0;
    if (copy >= 0) close(copy);
    uint64_t kb = 0;
    int number = 0;
    char name[16];
    int fields = sscanf("VmRSS:   123456 kB -17", "%15s %" SCNu64 " kB %d", name, &kb, &number);
    total += (unsigned)fields * 10000 + (unsigned)kb + (unsigned)(-number) + (unsigned char)name[2];
    return total;
}
}
