// In a class template's instance, a member type of a base that is no
// template's instance (so no dependent base of the pattern) hides a
// namespace's name: `using Result = Expected<SuccessType, ErrorType>` of
// JSC's wasm Parser<SuccessType>, ErrorType its base ParserBase's String,
// not the enum JSC::ErrorType.
#include <stdio.h>
namespace J {
enum class ErrorType : unsigned char { A, B };
struct Str { const char* s; };
class ParserBase { public: typedef Str ErrorType; };
template<typename S> class Parser : public ParserBase { public: using Result = ErrorType; };
}
int main() { J::Parser<void>::Result r { "ok" }; printf("%s %zu\n", r.s, sizeof(r)); return 0; }
