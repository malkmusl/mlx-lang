/* Typedefs, enums, sizeof, _Generic, statement expressions, recursion,
   compound literals, __func__ and the preprocessor. */
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <limits.h>

typedef struct list { int value; struct list *next; } List;
typedef int (*Binary)(int, int);
enum color { RED, GREEN = 10, BLUE, ALPHA = BLUE * 2 };
typedef enum { SMALL = -1, LARGE = 1 << 20 } size_kind;

#define SQUARE(x) ((x) * (x))
#define STR(x) #x
#define XSTR(x) STR(x)
#define CAT(a, b) a##b
#define COUNT(...) count_args(__VA_ARGS__, 0)
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#if defined(__mlxcc__) || defined(__GNUC__)
#define COMPILER "known"
#else
#define COMPILER "unknown"
#endif

static int count_args(int first, ...) { return first; }
static int fib(int n) { return n < 2 ? n : fib(n - 1) + fib(n - 2); }
static int apply(Binary f, int a, int b) { return f(a, b); }
static int sub(int a, int b) { return a - b; }
static const char *name(void) { return __func__; }
static int factorial(int n) { if (n <= 1) return 1; return n * factorial(n - 1); }

#define TYPE_NAME(x) _Generic((x), int: "int", double: "double", char *: "string", default: "other")

_Static_assert(sizeof(int) == 4, "int is 32 bits");

int main(void) {
    List c = { 3, NULL }, b = { 2, &c }, a = { 1, &b };
    int total = 0;
    for (List *p = &a; p; p = p->next) total += p->value;
    printf("%d\n", total);
    printf("%d %d %d %d\n", RED, GREEN, BLUE, ALPHA);
    size_kind k = LARGE;
    printf("%d %d\n", k, (int)sizeof(k));
    printf("%d %s %s\n", SQUARE(1 + 2), STR(a + b), XSTR(__LINE__));
    int CAT(var, 1) = 11;
    printf("%d %d\n", var1, COUNT(5, 6, 7));
    printf("%d\n", MAX(3, 9));
    printf("%s\n", COMPILER);
    printf("%d %d\n", fib(20), factorial(10));
    printf("%d\n", apply(sub, 10, 3));
    printf("%s\n", name());
    printf("%s %s %s %s\n", TYPE_NAME(1), TYPE_NAME(1.0), TYPE_NAME("s"), TYPE_NAME('c'));
    int v = ({ int t = 5; t * 2; });
    printf("%d\n", v);
    int *cl = (int[]){ 4, 5, 6 };
    printf("%d\n", cl[1] + ((struct { int x; }){ 9 }).x);
    bool flag = true;
    printf("%d %d\n", flag, (int)sizeof(bool));
    int64_t i64 = INT64_MAX;
    uint8_t u8 = 255;
    u8++;
    printf("%ld %d\n", i64, u8);
    char text[16];
    memset(text, 'x', sizeof text);
    text[5] = 0;
    printf("%s %d\n", text, (int)sizeof(text));
    int arr[] = { 1, 2, 3, 4 };
    printf("%d %d\n", (int)(sizeof arr / sizeof *arr), (int)sizeof(arr[0] + 1L));
    printf("%d %d\n", __LINE__, (int)strlen(__FILE__) > 0);
    const int ci = 4;
    volatile int vi = ci * 2;
    printf("%d\n", vi);
    unsigned long mask = ~0ul >> (sizeof(long) * CHAR_BIT - 12);
    printf("%lx\n", mask);
    return 0;
}
