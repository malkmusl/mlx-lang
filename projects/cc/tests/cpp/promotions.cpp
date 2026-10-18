// Integral promotions in overload resolution ([conv.prom]): char32_t
// promotes to unsigned int, wchar_t, char16_t and char8_t to int (JSC's
// jsNumber(codePoint) picked jsNumber(char)); and std::span converts
// only to spans of compatible elements (WTF's String{span<char>}).
#include <cstdio>
#include <span>
const char* f(char) { return "char"; }
const char* f(unsigned char) { return "uchar"; }
const char* f(short) { return "short"; }
const char* f(unsigned short) { return "ushort"; }
const char* f(int) { return "int"; }
const char* f(unsigned) { return "unsigned"; }
const char* f(long) { return "long"; }
const char* f(unsigned long) { return "ulong"; }
const char* f(double) { return "double"; }
const char* g(std::span<const char16_t>) { return "u16"; }
const char* g(std::span<const unsigned char>) { return "latin1"; }
const char* g(std::span<const char>) { return "char"; }
const char* g(std::span<const char8_t>) { return "u8"; }
int main() {
    char32_t c = 0x1D4B3; char8_t e = 0xE9; char16_t h = 0x1E69; wchar_t w = 1; bool b = true; unsigned short us = 2;
    printf("%s %s %s %s %s %s\n", f(c), f(e), f(h), f(w), f(b), f(us));
    char buffer[4] = "ab"; std::span<char> s { buffer }; std::span<char16_t> u;
    printf("%s %s\n", g(s), g(u));
    return 0;
}
