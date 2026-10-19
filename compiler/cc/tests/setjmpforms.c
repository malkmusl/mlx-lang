// setjmp's second return (from longjmp) in the forms code takes it in:
// assigned (`r = setjmp(buf);`, which kept the variable's address on the
// stack across the call, overwritten by the calls before the longjmp:
// a crash), compared, negated, initializing, as a switch's value.
#include <stdio.h>
#include <setjmp.h>

static jmp_buf buf;
static void jump(int v) { longjmp(buf, v); }
static void deeper(int v) { char pad[256]; snprintf(pad, sizeof pad, "%d", v); jump(v + pad[0] - '0'); }

int main(int argc, char **argv)
{
    volatile int count = 0;
    int r;
    r = setjmp(buf);
    printf("r=%d\n", r);
    if (r < 3) {
        count++;
        deeper(r);
    }
    printf("count=%d\n", count);
    if (setjmp(buf) == 0)
        deeper(1);
    else
        printf("compared\n");
    if (!setjmp(buf))
        deeper(2);
    printf("negated\n");
    int s = setjmp(buf);
    if (!s)
        deeper(4);
    printf("s=%d\n", s);
    switch (setjmp(buf)) {
    case 0: deeper(7); break;
    case 14: printf("switch 14\n"); break;
    default: printf("switch other\n");
    }
    return argc;
}
