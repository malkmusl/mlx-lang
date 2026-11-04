// A qualified call `N::f(...)` weighs N's functions of the name only: not
// those of a namespace a using-directive elsewhere names, though that
// directive is in an unnamed namespace (simdutf: `namespace arm64 {
// namespace { using namespace simdutf; } }`, and in simdutf::fallback
// `scalar::utf8::utf16_length_from_utf8(p, n)`, a template of
// simdutf::scalar::(unnamed)::utf8, called simdutf's own
// utf16_length_from_utf8(const char *, size_t), which called it again).
#include <stddef.h>
#include <stdio.h>

namespace text {
namespace scalar {
namespace {
namespace utf8 {
template<typename Pointer>
constexpr size_t length(Pointer, size_t count) { return count * 10; }
}
}
}

size_t length(const char*, size_t);

namespace arm { namespace { using namespace text; } }

namespace fallback {
size_t measure(const char* input, size_t count) { return scalar::utf8::length(input, count); }
}

size_t length(const char* input, size_t count) { return fallback::measure(input, count) + 1; }
}

namespace { using namespace text; }

size_t measureHere(const char* input) { return text::scalar::utf8::length(input, 2); }

int main()
{
    printf("%zu %zu %zu\n", text::fallback::measure("abc", 3), text::length("ab", 2), measureHere("x"));
    return 0;
}
