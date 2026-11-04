// A bit-field's write reads and writes the bytes it occupies and no
// others: a member beside it that is no bit-field is a memory location
// of its own (C11 3.14, C++ [intro.memory]), which another thread may be
// writing. JSC's CodeBlock keeps its one-byte m_lock just before
// `unsigned m_capabilityLevelState : 2`; written as the whole unsigned,
// the field put back the lock's byte as it was read, under a JIT thread
// holding the lock (Voyager aborted in Lock::unlockSlow, "Invalid value
// for lock: 0"). The fields' values are checked in every unit and
// position, the neighbors' bytes after each write, and a lock byte
// beside a bit-field written from another thread at once.
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <stdbool.h>

struct Small { unsigned char lock; bool inlined; unsigned cap : 2; bool a : 1; bool b : 1; bool after; };
struct Wide { unsigned x : 20; char c; unsigned long long y : 37; unsigned short z; long long w : 27; signed char t; };
struct Cross { unsigned long long lo : 3; unsigned long long mid : 58; unsigned long long hi : 3; };

static unsigned long long sum(const void* p, size_t n)
{
    const unsigned char* bytes = p;
    unsigned long long s = 0;
    for (size_t i = 0; i < n; i++) s = s * 131 + bytes[i];
    return s;
}

static struct Small shared;
static volatile int stop;
static long wrong;
static void* locker(void* unused)
{
    (void)unused;
    for (long i = 0; i < 2000000; i++) {
        __atomic_store_n(&shared.lock, 1, __ATOMIC_SEQ_CST);
        if (__atomic_load_n(&shared.lock, __ATOMIC_SEQ_CST) != 1) wrong++;
        __atomic_store_n(&shared.lock, 0, __ATOMIC_SEQ_CST);
    }
    stop = 1;
    return 0;
}
static void* writer(void* unused)
{
    (void)unused;
    unsigned n = 0;
    while (!stop) { shared.cap = n & 3; shared.b = n & 1; n++; }
    return 0;
}

int main(void)
{
    struct Small s;
    memset(&s, 0xa5, sizeof s);
    s.cap = 2; s.a = 0; s.b = 1;
    printf("small %u %d %d lock %u inlined %u after %u size %zu\n", s.cap, s.a, s.b, s.lock, *(unsigned char*)&s.inlined, *(unsigned char*)&s.after, sizeof s);
    struct Wide w;
    memset(&w, 0x5a, sizeof w);
    w.x = 0xabcde; w.y = 0x1234567891ULL; w.w = -1234567; w.c = 7; w.z = 0x7777; w.t = -3;
    for (int i = 0; i < 40; i++) { w.x += 3 * i; w.y ^= (unsigned long long)i << (i % 30); w.w -= i; }
    printf("wide %x %d %llx %u %lld %d size %zu sum %llx\n", w.x, w.c, (unsigned long long)w.y, w.z, (long long)w.w, w.t, sizeof w, sum(&w, sizeof w));
    struct Cross c;
    memset(&c, 0, sizeof c);
    c.lo = 5; c.mid = 0x2aaaaaaaaaaaaaaULL; c.hi = 6;
    c.mid += 1;
    printf("cross %llu %llx %llu sum %llx\n", (unsigned long long)c.lo, (unsigned long long)c.mid, (unsigned long long)c.hi, sum(&c, sizeof c));
    pthread_t a, b;
    pthread_create(&b, 0, writer, 0);
    pthread_create(&a, 0, locker, 0);
    pthread_join(a, 0);
    pthread_join(b, 0);
    printf("lock beside a bit-field written at once: %s\n", wrong ? "overwritten" : "kept");
    return 0;
}
