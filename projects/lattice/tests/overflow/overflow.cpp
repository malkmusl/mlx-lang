// The overflow test: __builtin_add_overflow, __builtin_sub_overflow and
// __builtin_mul_overflow over 32 and 64 bits, signed and unsigned, at the
// edges of each type; and with operands and a result of different types
// (the front end computes those in a wide integer).
namespace overflow {
static unsigned mix(unsigned h, unsigned long long v) {
    h ^= (unsigned)(v ^ (v >> 32));
    h *= 16777619u;
    return h;
}
template <class T> static unsigned mul(unsigned h, T a, T b) {
    T r = 0;
    bool o = __builtin_mul_overflow(a, b, &r);
    h = mix(h, (unsigned long long)r);
    return mix(h, o ? 7 : 3);
}
template <class T> static unsigned add(unsigned h, T a, T b) {
    T r = 0;
    bool o = __builtin_add_overflow(a, b, &r);
    h = mix(h, (unsigned long long)r);
    return mix(h, o ? 7 : 3);
}
template <class T> static unsigned sub(unsigned h, T a, T b) {
    T r = 0;
    bool o = __builtin_sub_overflow(a, b, &r);
    h = mix(h, (unsigned long long)r);
    return mix(h, o ? 7 : 3);
}
static unsigned mixed(unsigned h, unsigned long long a, unsigned b) {
    unsigned long long r = 0;
    bool o = __builtin_mul_overflow(a, b, &r);
    h = mix(h, r);
    return mix(h, o ? 7 : 3);
}
static unsigned narrow(unsigned h, long long a, long long b) {
    int r = 0;
    bool o = __builtin_add_overflow(a, b, &r);
    h = mix(h, (unsigned long long)(long long)r);
    return mix(h, o ? 7 : 3);
}
static unsigned crossed(unsigned h, int a, unsigned long long b) {
    long long r = 0;
    bool o = __builtin_sub_overflow(a, b, &r);
    h = mix(h, (unsigned long long)r);
    return mix(h, o ? 7 : 3);
}
int run() {
    unsigned h = 2166136261u;
    h = mul<unsigned long long>(h, 0xFFFFFFFFFFFFFFFFull, 0xFFFFFFFFFFFFFFFFull);
    h = mul<unsigned long long>(h, 0x100000000ull, 0x100000000ull);
    h = mul<unsigned long long>(h, 0xFFFFFFFFull, 0xFFFFFFFFull);
    h = mul<unsigned long long>(h, 0x8000000000000000ull, 2);
    h = mul<long long>(h, -5, 7);
    h = mul<long long>(h, 0x7FFFFFFFFFFFFFFFll, 2);
    h = mul<long long>(h, -0x7FFFFFFFFFFFFFFFll - 1, -1);
    h = mul<long long>(h, 3037000500ll, 3037000500ll);
    h = mul<int>(h, 65536, 65536);
    h = mul<int>(h, -46341, 46341);
    h = mul<unsigned>(h, 65536u, 65535u);
    h = mul<unsigned>(h, 65536u, 65536u);
    h = add<unsigned long long>(h, 0xFFFFFFFFFFFFFFFFull, 1);
    h = add<unsigned long long>(h, 0xFFFFFFFFFFFFFFFEull, 1);
    h = add<long long>(h, 0x7FFFFFFFFFFFFFFFll, 1);
    h = add<long long>(h, -0x7FFFFFFFFFFFFFFFll - 1, -1);
    h = add<int>(h, 2147483647, 1);
    h = add<unsigned>(h, 4294967295u, 1u);
    h = sub<unsigned long long>(h, 0, 1);
    h = sub<unsigned long long>(h, 5, 5);
    h = sub<long long>(h, -0x7FFFFFFFFFFFFFFFll - 1, 1);
    h = sub<int>(h, -2147483647 - 1, 1);
    h = sub<unsigned>(h, 3u, 4u);
    h = mixed(h, 0xFFFFFFFFFFFFFFFFull, 2u);
    h = mixed(h, 0x100000000ull, 0xFFFFFFFFu);
    h = mixed(h, 0xFFFFFFFFFFFFFFFFull, 0xFFFFFFFFu);
    h = narrow(h, 2147483647ll, 0);
    h = narrow(h, 2147483647ll, 1);
    h = narrow(h, -0x7FFFFFFFFFFFFFFFll - 1, -1);
    h = narrow(h, 0x100000000ll, -0x100000000ll);
    h = crossed(h, -1, 0xFFFFFFFFFFFFFFFFull);
    h = crossed(h, 5, 7ull);
    h = crossed(h, 2147483647, 0x8000000000000000ull);
    return (int)h;
}
}
