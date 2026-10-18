// The C++ ABI's run-time functions programs call (include/cxxabi.h).
#include <cxxabi.h>

// No demangling: the name is reported invalid (status -2), and the
// caller keeps the mangled one.
extern "C" char *__cxa_demangle(const char *mangled, char *buffer, size_t *length, int *status) {
    (void)mangled;
    (void)buffer;
    (void)length;
    if (status) *status = -2;
    return nullptr;
}

// Static locals made once whatever the threads (mlx-cc's guard: two
// words, the first's low byte set once the object is made, the second
// the thread making it). The object is made by the thread that takes
// the guard; the others wait for it. The same thread coming back while
// making it (a recursive initialization) makes it again, as an
// unguarded static would.
#include <pthread.h>
#include <sched.h>

extern "C" int __mlxcc_guard_acquire(unsigned long *guard) {
    unsigned long self = (unsigned long)pthread_self();
    for (;;) {
        if (__atomic_load_n((unsigned char *)guard, __ATOMIC_ACQUIRE)) return 0;
        unsigned long maker = 0;
        if (__atomic_compare_exchange_n(guard + 1, &maker, self, false, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) {
            if (__atomic_load_n((unsigned char *)guard, __ATOMIC_ACQUIRE)) {
                __atomic_store_n(guard + 1, 0UL, __ATOMIC_RELEASE);
                return 0;
            }
            return 1;
        }
        if (maker == self) return 1;
        sched_yield();
    }
}

extern "C" void __mlxcc_guard_release(unsigned long *guard) {
    __atomic_store_n((unsigned char *)guard, (unsigned char)1, __ATOMIC_RELEASE);
    __atomic_store_n(guard + 1, 0UL, __ATOMIC_RELEASE);
}
