// Conversion functions: one template converting to `R<To>` is more
// specialized than one converting to a bare `To`, and a conversion
// function that is no template beats a template's instance (JSC's
// ExceptionOperationImplicitResult: the bare `operator To()` made every
// JIT operation's ExceptionOperationResult of its value alone, the
// exception lost, and a direct eval's exception escaped its try).
#include <stdio.h>
#include <stdint.h>
#include <type_traits>
struct Exception;
template<typename T> struct Result { T value; Exception* exception; };
template<typename From> struct Implicit {
    template<typename To> requires std::is_convertible_v<From, To>
    operator Result<To>() { Result<To> r; r.value = value; r.exception = exception; return r; }
    template<typename To> operator To() { return value; }
    From value;
    Exception* exception;
};
struct Plain { long value; Exception* exception; };
struct Both {
    template<typename To> operator To() { return To { value, nullptr }; }
    operator Plain() { Plain p; p.value = value; p.exception = exception; return p; }
    long value;
    Exception* exception;
};
__attribute__((noinline)) Result<int64_t> returned(Implicit<int64_t> i) { return i; }
__attribute__((noinline)) Result<int64_t> direct(Implicit<int64_t> i) { Result<int64_t> r(i); return r; }
__attribute__((noinline)) Result<int64_t> copied(Implicit<int64_t> i) { Result<int64_t> r = i; return r; }
__attribute__((noinline)) Plain plain(Both b) { Plain p(b); return p; }
int main()
{
    Implicit<int64_t> i { 7, (Exception*)0x99 };
    Both b { 8, (Exception*)0x77 };
    int64_t fromBare = i;
    printf("%d %d %d %d %ld %ld\n", returned(i).exception == (Exception*)0x99, direct(i).exception == (Exception*)0x99, copied(i).exception == (Exception*)0x99, plain(b).exception == (Exception*)0x77, (long)returned(i).value, (long)fromBare);
    return 0;
}
