/* Statements: if, loops, switch, goto, break and continue. */
#include <stdio.h>

static int classify(int n) {
    switch (n) {
    case 0: return 100;
    case 1:
    case 2: return 200;
    case 3 ... 6: return 300;
    case -1: return -100;
    default: return 999;
    }
}

static int fallthrough(int n) {
    int r = 0;
    switch (n) {
    case 1: r += 1;
    case 2: r += 10;
    case 3: r += 100; break;
    case 4: r += 1000;
    }
    return r;
}

int main(void) {
    int sum = 0;
    for (int i = 0; i < 10; i++) {
        if (i == 3) continue;
        if (i == 8) break;
        sum += i;
    }
    printf("for %d\n", sum);
    int n = 0;
    while (n < 100) n = n * 2 + 1;
    printf("while %d\n", n);
    int k = 10;
    do { k -= 3; } while (k > 0);
    printf("do %d\n", k);
    for (int i = -2; i < 9; i++) printf("%d ", classify(i));
    printf("\n");
    for (int i = 0; i < 6; i++) printf("%d ", fallthrough(i));
    printf("\n");
    int count = 0;
again:
    count++;
    if (count < 5) goto again;
    printf("goto %d\n", count);
    int total = 0;
    for (int a = 0; a < 5; a++)
        for (int b = 0; b < 5; b++) {
            if (b > a) break;
            total += a * b;
        }
    printf("nested %d\n", total);
    int x = 5, y = 0;
    if (x > 3 && (y = 1)) printf("and %d\n", y);
    if (x < 3 || (y = 2)) printf("or %d\n", y);
    if (x < 3 && (y = 3)) printf("never\n");
    printf("y %d\n", y);
    int c = (x++, x++, x);
    printf("comma %d %d\n", c, x);
    int m = x > 6 ? x < 8 ? 1 : 2 : 3;
    printf("ternary %d\n", m);
    for (;;) { if (++m > 4) break; }
    printf("forever %d\n", m);
    int i = 0;
    while (1) {
        switch (i) {
        case 0: i = 5; continue;
        case 5: i = 9; break;
        }
        if (i == 9) break;
    }
    printf("switch in loop %d\n", i);
    {
        int shadow = 1;
        {
            int shadow = 2;
            printf("inner %d\n", shadow);
        }
        printf("outer %d\n", shadow);
    }
    return 3;
}
