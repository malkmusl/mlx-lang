// What follows a return, break, continue or goto up to a label is not
// generated (as Clang and GCC do not): a function named only there needs
// no definition (JavaScriptCore's B3LowerMacros.cpp calls Math::i64_div_u,
// defined for 32-bit ARM only, after a break that ends its case). A
// label after the jump is reached as before: a goto's, a case's (Duff's
// device), one inside a block or a loop.
#include <stdio.h>

int never_defined(int);

static int pick(int op, int x)
{
    switch (op) {
    case 0: {
        if (1)
            break;
        return never_defined(x);
    }
    case 1:
        return x * 2;
        never_defined(x);
    case 2:
        x += 1;
        goto done;
        x = never_defined(x);
    done:
        return x + 100;
    }
    return -1;
}

static int duff(int count)
{
    int total = 0;
    int n = (count + 3) / 4;
    switch (count % 4) {
    case 0: do { total += 1;
    case 3:      total += 1;
    case 2:      total += 1;
    case 1:      total += 1;
            } while (--n > 0);
    }
    return total;
}

static int skipping(int x)
{
    for (int i = 0; i < 3; i++) {
        if (i == 1)
            continue;
        x += i;
        continue;
        x = never_defined(x);
    }
    goto inner;
    return never_defined(x);
    {
        x *= 1000;
    inner:
        x += 7;
    }
    return x;
}

int main(void)
{
    printf("%d %d %d %d %d %d\n", pick(0, 5), pick(1, 5), pick(2, 5), duff(7), duff(8), skipping(1));
    return 0;
}
