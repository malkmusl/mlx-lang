// References to const bound to values of another type: a std::string
// from a literal, a class from an int, a temporary string; each is a
// temporary made for the reference, which lives as long as it (the
// conformance suite's extension names).
#include <stdio.h>
#include <string>

struct Holder
{
    Holder(int v) : value(v)
    {
    }
    int value;
};

static size_t length(const std::string &s)
{
    return s.size();
}

int main()
{
    const std::string &name = "VK_EXT_descriptor_indexing";
    const Holder &h         = 7;
    const std::string &joined = name + "!";
    printf("%s %zu %d %s %zu\n", name.c_str(), name.size(), h.value, joined.c_str(), length("abc"));
    return 0;
}
