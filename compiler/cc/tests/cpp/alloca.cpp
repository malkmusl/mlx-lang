// alloca: room on the stack until the function returns, also in the
// middle of an expression whose other operands are being evaluated (as
// GLib's g_alloca in JavaScriptCore's GLib API: a span made of it among
// a call's arguments), several times in one function, in recursion.
#include <alloca.h>
#include <stdio.h>
#include <string.h>

static int fill(int* values, int count, int start)
{
    for (int i = 0; i < count; i++)
        values[i] = start + i;
    int sum = 0;
    for (int i = 0; i < count; i++)
        sum += values[i];
    return sum;
}

static int combine(int a, int b, int c, int d) { return a * 1000000 + b * 1000 + c + d; }

static int depthSum(int level)
{
    if (!level)
        return 0;
    char* text = static_cast<char*>(alloca(level + 1));
    memset(text, 'a' + level, level);
    text[level] = 0;
    int inner = depthSum(level - 1);
    return inner + (int)strlen(text) + (text[0] - 'a');
}

struct Span {
    int* data;
    int size;
};

static Span makeSpan(int* data, int size) { return Span { data, size }; }

int main(int argc, char** argv)
{
    int count = argc + 4;
    // Operands pushed before the alloca, then evaluated after it.
    int value = combine(count, fill(static_cast<int*>(alloca(count * sizeof(int))), count, 10), count * 2, argc);
    printf("combined %d\n", value);
    Span span = makeSpan(static_cast<int*>(alloca(sizeof(int) * count)), count);
    printf("span sum %d\n", fill(span.data, span.size, 100));
    long total = 0;
    for (int round = 0; round < 50; round++) {
        double* room = static_cast<double*>(alloca(sizeof(double) * 8));
        for (int i = 0; i < 8; i++)
            room[i] = round + i * 0.5;
        total += (long)(room[7] * 2);
        if (((unsigned long)room & 15) != 0)
            printf("not aligned\n");
    }
    printf("total %ld\n", total);
    printf("recursion %d\n", depthSum(10));
    printf("span still %d %d\n", span.data[0], span.data[span.size - 1]);
    return argc - 1;
}
