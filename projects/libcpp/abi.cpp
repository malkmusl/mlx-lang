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
