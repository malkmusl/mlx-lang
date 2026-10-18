/* Arrays, pointers, pointer arithmetic and function pointers. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int add(int a, int b) { return a + b; }
static int mul(int a, int b) { return a * b; }
static int (*pick(int which))(int, int) { return which ? mul : add; }

static int compare(const void *a, const void *b) {
    return *(const int *)a - *(const int *)b;
}

static void fill(int *p, int n) { for (int i = 0; i < n; i++) p[i] = i * i; }

int main(void) {
    int a[10];
    fill(a, 10);
    int *p = a + 3;
    printf("%d %d %d %d\n", *p, p[2], *(p - 1), 5[a]);
    printf("%ld %ld\n", (long)(&a[9] - p), (long)(p - &a[9]));
    int m[3][4];
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 4; j++) m[i][j] = i * 10 + j;
    printf("%d %d %d\n", m[2][3], *(*(m + 1) + 2), (int)sizeof(m[1]));
    int (*row)[4] = m + 1;
    printf("%d\n", row[1][0]);
    char *names[] = { "zero", "one", "two" };
    char **q = names;
    printf("%s %s %c\n", *q, q[2], *names[1]);
    int (*ops[2])(int, int) = { add, mul };
    printf("%d %d %d %d\n", ops[0](6, 7), ops[1](6, 7), pick(0)(2, 3), (*pick(1))(2, 3));
    int values[] = { 42, 7, 19, -3, 0, 88, 7 };
    qsort(values, sizeof(values) / sizeof(values[0]), sizeof(int), compare);
    for (int i = 0; i < 7; i++) printf("%d ", values[i]);
    printf("\n");
    int *heap = malloc(5 * sizeof *heap);
    for (int i = 0; i < 5; i++) heap[i] = 100 + i;
    heap = realloc(heap, 10 * sizeof(int));
    printf("%d %d\n", heap[0], heap[4]);
    free(heap);
    char buf[32];
    strcpy(buf, "hello");
    strcat(buf, ", world");
    printf("%s %d %d\n", buf, (int)strlen(buf), strcmp(buf, "hello") > 0);
    char *e = buf + strlen(buf);
    while (e > buf && *--e != ' ') ;
    printf("%s\n", e);
    void *v = buf;
    char *cp = v;
    printf("%c\n", cp[1]);
    int x = 1;
    int *px = &x, **ppx = &px;
    **ppx = 9;
    printf("%d\n", x);
    printf("%d\n", px == &x && px != 0 && !(px == 0));
    long diff = (char *)&a[5] - (char *)&a[0];
    printf("%ld\n", diff);
    return 0;
}
