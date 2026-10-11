/* Globals and their initializers, statics, string literals, and
   objects shared with the second file (globals2.c). */
#include <stdio.h>

extern int shared;
extern const char *message;
int counter(void);
int helper(int);

int a = 5;
int *pa = &a;
int arr[5] = { 1, 2, 3 };
int *mid = &arr[2];
int *end = arr + 5;
char str[] = "array";
char *ptr = "pointer";
const char *list[] = { "x", "yy", "zzz", 0 };
struct entry { const char *name; int value; int (*fn)(int); } entries[] = {
    { "one", 1, helper },
    { "two", 2, 0 },
};
long sizes[] = { sizeof(int), sizeof(long), sizeof(entries) };
int tentative;
int tentative;
static int hidden = 7;
char *strs[2][2] = { { "a", "b" }, { "c", "d" } };
int matrix[2][3] = { 1, 2, 3, 4, 5, 6 };
unsigned char bytes[] = { 255, 128, 0 };
short shorts[4] = { [3] = -1 };
long diff = (long)(&arr[4] - &arr[1]);

static int bump(void) {
    static int calls = 10;
    return ++calls;
}

int main(void) {
    printf("%d %d %d %d\n", a, *pa, *mid, (int)(end - arr));
    printf("%s %s %d %d\n", str, ptr, (int)sizeof str, (int)sizeof ptr);
    for (const char **p = list; *p; p++) printf("%s ", *p);
    printf("\n");
    printf("%s %d %d %s %d\n", entries[0].name, entries[0].fn(3), entries[1].value, entries[1].name, entries[1].fn == 0);
    printf("%ld %ld %ld\n", sizes[0], sizes[1], sizes[2]);
    printf("%d %d\n", tentative, hidden);
    printf("%s%s%s%s\n", strs[0][0], strs[0][1], strs[1][0], strs[1][1]);
    printf("%d %d %d\n", matrix[1][0], matrix[0][2], bytes[0] + bytes[1]);
    printf("%d %d %ld\n", shorts[0], shorts[3], diff);
    printf("%d %d %d\n", bump(), bump(), bump());
    printf("%d %s %d %d\n", shared, message, counter(), counter());
    shared = 99;
    printf("%d\n", helper(1));
    *pa = 6;
    printf("%d\n", a);
    return 0;
}
