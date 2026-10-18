// std::source_location::current(): its default arguments give the place
// of the call (WebCore's internalError(const URL&, std::source_location =
// std::source_location::current())), in a function's own default argument
// the place of that function's call.
#include <source_location>
#include <cstdio>
#include <cstring>

static unsigned where(std::source_location location = std::source_location::current())
{
    return location.line();
}

struct Logger {
    unsigned last = 0;
    void log(const char* message, std::source_location location = std::source_location::current())
    {
        last = location.line();
        std::printf("%s at line %u\n", message, last);
    }
};

static bool endsWith(const char* text, const char* suffix)
{
    size_t length = std::strlen(text), suffixLength = std::strlen(suffix);
    return length >= suffixLength && !std::strcmp(text + length - suffixLength, suffix);
}

int main(int argc, char**)
{
    std::source_location here = std::source_location::current();
    std::printf("here %u\n", here.line());
    std::printf("file %d\n", endsWith(here.file_name(), "sourcelocation.cpp"));
    std::printf("where %u\n", where());
    std::printf("where %u\n",
        where());
    Logger logger;
    logger.log("first");
    auto lambda = [&] {
        logger.log("from a lambda");
    };
    lambda();
    std::source_location empty;
    std::printf("empty %u %d\n", empty.line(), empty.file_name()[0]);
    return logger.last % 7 + argc;
}
