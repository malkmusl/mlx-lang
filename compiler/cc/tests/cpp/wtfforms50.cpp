// A function template's default argument given on its declaration (size_t = sizeof(T), JSC's allocateCell) read with each instance's T
#include <stdio.h>
#include <stddef.h>

namespace p_defarg {
struct VM {};
struct Ctx {};
template<typename T> size_t allocateCell(VM&, size_t = sizeof(T));
template<typename T> size_t allocateCell(VM&, Ctx*, size_t = sizeof(T));
struct Small { long a, b; };
struct Big { long a[14]; };
template<typename T> size_t allocateCell(VM&, size_t size) { return size; }
template<typename T> size_t allocateCell(VM&, Ctx*, size_t size) { return size + 1000; }
struct Global { long x[5]; static size_t create(VM& vm) { return allocateCell<Global>(vm); } };
int run() {
    VM vm; Ctx c;
    size_t a = allocateCell<Small>(vm), b = allocateCell<Big>(vm), d = allocateCell<Big>(vm, &c), e = Global::create(vm), f = allocateCell<Small>(vm, 3);
    printf("%zu %zu %zu %zu %zu\n", a, b, d, e, f);
    return 0;
}
}

namespace p_defarg2 {
struct VM2 {};
struct Small { long a, b; };
struct Big { long a[14]; };
template<typename T> size_t one(VM2&, size_t size = sizeof(T)) { return size; }
template<typename T> size_t two(VM2&, size_t = sizeof(T));
template<typename T> size_t two(VM2&, size_t size) { return size; }
template<typename T> size_t three(VM2&, size_t = sizeof(T));
int run() {
    VM2 vm;
    printf("%zu %zu %zu %zu\n", one<Small>(vm), one<Big>(vm), two<Small>(vm), two<Big>(vm));
    return 0;
}
}

int main() {
    p_defarg::run();
    p_defarg2::run();
    return 0;
}
