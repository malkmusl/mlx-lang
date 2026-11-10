// The translator's test of: see tools/check_lattice_emit.sh (variadic).
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
namespace variadic {
static unsigned hashText(const char *text)
{
    unsigned h = 5381;
    for (const char *p = text; *p; p++) h = h * 33 + (unsigned char)*p;
    return h;
}
static int formatInto(char *buffer, size_t size, const char *format, va_list arguments)
{
    va_list copy;
    va_copy(copy, arguments);
    int needed = vsnprintf(nullptr, 0, format, copy);
    va_end(copy);
    int written = vsnprintf(buffer, size, format, arguments);
    return needed * 1000 + written;
}
static int formatted(char *buffer, size_t size, const char *format, ...)
{
    va_list arguments;
    va_start(arguments, format);
    int result = formatInto(buffer, size, format, arguments);
    va_end(arguments);
    return result;
}
static long sumOf(int count, ...)
{
    va_list arguments;
    va_start(arguments, count);
    long total = 0;
    for (int i = 0; i < count; i++)
    {
        if (i % 3 == 0) total += va_arg(arguments, int);
        else if (i % 3 == 1) total += (long)(va_arg(arguments, double) * 4);
        else total += (long)strlen(va_arg(arguments, const char *));
    }
    va_end(arguments);
    return total;
}
struct Logger
{
    char text[96];
    int calls = 0;
    void log(const char *format, ...)
    {
        va_list arguments;
        va_start(arguments, format);
        vsnprintf(text, sizeof(text), format, arguments);
        va_end(arguments);
        calls++;
    }
};
unsigned run()
{
    char buffer[64];
    unsigned total = 0;
    int r = formatted(buffer, sizeof(buffer), "%d|%5.2f|%s|%u|%x|%c|%ld", -42, 2.5, "abc", 77u, 255, 'z', -9000000000L);
    total += hashText(buffer) + (unsigned)r;
    float single = 1.25f;
    bool flag = true;
    unsigned char small = 200;
    short negative = -3;
    int n = snprintf(buffer, sizeof(buffer), "%.3f %d %u %d %p", single, flag, small, negative, (void *)nullptr);
    total += hashText(buffer) * 3 + (unsigned)n;
    snprintf(buffer, 8, "%s", "a long text cut short");
    total += hashText(buffer);
    total += (unsigned)sumOf(5, 10, 2.5, "four", -3, 0.75);
    total += (unsigned)sumOf(0);
    Logger logger;
    logger.log("%s=%d", "value", 17);
    logger.log("%llu", 12345678901234ull);
    total += hashText(logger.text) + (unsigned)logger.calls;
    sprintf(buffer, "%08.3f|%-6d|%+d", 3.14159, 12, 5);
    total += hashText(buffer);
    return total;
}
}
