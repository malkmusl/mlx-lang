// tools/check_crash_report.sh: the crash is in this object's copy of the
// inline twice(), which crash_report_inline_a.cpp's replaced as the
// program's definition; the report must still name it.
#include <stdio.h>
inline int twice(int *p) { return *p * 2; }
int fromA(int *p);
int fromB(int *p) { return twice(p) + 2; }
int main(int argc, char **argv) {
    int x = argc;
    printf("%d\n", fromA(&x));
    fflush(stdout);
    return fromB(argc > 5 ? &x : (int *)0);
}
