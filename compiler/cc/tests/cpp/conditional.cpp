// The conditional operator's type in C++ ([expr.cond]): two operands of
// one type give that type, with no promotion (x ? '-' : '+' is a char).
#include <cstdio>
template<typename T> const char* kind(T) { return "other"; }
template<> const char* kind(char) { return "char"; }
template<> const char* kind(int) { return "int"; }
template<> const char* kind(bool) { return "bool"; }
template<> const char* kind(short) { return "short"; }
int main(int argc, char**) {
    char a = 'a';
    short s = 1;
    printf("%s %s %s %s %s %d\n", kind(argc < 0 ? '-' : '+'), kind(argc ? true : false), kind(argc ? s : s), kind(argc ? a : 'b'), kind(argc ? 'a' : 1), (int)sizeof(argc ? 'x' : 'y'));
    return 0;
}
