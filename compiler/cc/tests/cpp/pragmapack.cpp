// `#pragma pack(push, N)` / `pack(pop)` / `pack(N)` (Skia's OpenType table
// structs): the members' alignment capped while it is in force.
#include <cstdio>
#include <cstdint>
#pragma pack(push, 1)
struct A {
    uint16_t version;
    struct W {
        enum Value : uint16_t { Thin = 100, Bold = 700 } value;
    } width;
    union T {
        struct Field {
            uint8_t r8 : 1, r9 : 1, r10 : 1, r11 : 1, r12 : 1, r13 : 1, r14 : 1, r15 : 1;
            uint8_t r0 : 1, r1 : 1, r2 : 1, r3 : 1, r4 : 1, r5 : 1, r6 : 1, r7 : 1;
        } field;
        struct Raw { static const uint16_t Mask = 2; uint16_t value; } raw;
    } fsType;
    uint32_t range[4];
    char vendor[4];
    uint16_t last;
};
#pragma pack(pop)
struct B { char c; int i; };
#pragma pack(2)
struct C { char c; int i; double d; };
#pragma pack()
struct D { char c; double d; };
enum class E16 : uint16_t { a, b };
int main(int argc, char **argv) {
    printf("%zu %zu %zu %zu %zu %zu\n", sizeof(A), sizeof(A::W), sizeof(A::T), sizeof(A::T::Field), sizeof(B), sizeof(E16));
    printf("%zu %zu %zu\n", sizeof(C), alignof(C), sizeof(D));
    return argc;
}
