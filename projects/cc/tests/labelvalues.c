/* GNU C's labels as values: `&&label` is the label's address (a
   `void *`), `goto *p` jumps to it; a static table of them is
   relocated against the code. Interpreters (Lua, JavaScriptCore's C
   loop) dispatch so under __GNUC__. */
#include <stdio.h>

enum { INC, DOUBLE, SQUARE, END };

static int run(const unsigned char *code)
{
    static const void *const table[] = { &&op_inc, &&op_double, &&op_square, &&op_end };
    const void *local[] = { &&op_inc, &&op_double, &&op_square, &&op_end };
    int acc = 1;
    const unsigned char *pc = code;
    goto *table[*pc++];
op_inc:
    acc += 1;
    goto *local[*pc++];
op_double:
    acc *= 2;
    goto *table[*pc++];
op_square:
    acc *= acc;
    goto *local[*pc++];
op_end:
    return acc;
}

static int count(int n)
{
    void *again = &&loop;
    int steps = 0;
loop:
    steps++;
    if (--n > 0) goto *again;
    return steps;
}

int main(void)
{
    unsigned char program[] = { INC, DOUBLE, DOUBLE, INC, SQUARE, END };
    unsigned char other[] = { SQUARE, INC, END };
    printf("%d %d\n", run(program), run(other));
    printf("%d\n", count(7));
#ifdef __GNUC__
    printf("__GNUC__ %d\n", __GNUC__ >= 4);
#endif
    return 0;
}
