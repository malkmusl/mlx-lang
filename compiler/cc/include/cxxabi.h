/* <cxxabi.h>: the C++ ABI's run-time functions programs call: name
   demangling (mlxlibcpp has none: __cxa_demangle reports an invalid
   name, status -2, and the caller keeps the mangled one). */
#ifndef _MLXCC_CXXABI_H
#define _MLXCC_CXXABI_H
#include <stddef.h>
#ifdef __cplusplus
namespace abi {
extern "C" char *__cxa_demangle(const char *mangled, char *buffer, size_t *length, int *status);
}
using namespace abi;
#endif
#endif
