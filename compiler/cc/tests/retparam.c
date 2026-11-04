/* A struct returned in registers, built from a parameter after a call:
   region_loads folds the load of its first word to the parameter, which
   copy propagation puts into the returned tuple, and the back end read
   the tuple's components from their slots, where a parameter kept in its
   home register has none (Skia's SkString::appendf crashed so). */
#include <stdio.h>
#include <string.h>
struct buffer { char *text; int length; };
__attribute__((noinline)) static struct buffer format(const char *what, char *stack, int size)
{
    int length = snprintf(stack, size, "<%s>", what);
    if (length < 0)
        return (struct buffer){ stack, 0 };
    if (length < size)
        return (struct buffer){ stack, length };
    return (struct buffer){ 0, length };
}
int main(void)
{
    char stack[64];
    struct buffer b = format("abc", stack, sizeof stack);
    printf("%s %d %d\n", b.text ? b.text : "(null)", b.length, b.text == stack);
    return 0;
}
