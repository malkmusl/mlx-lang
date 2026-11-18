// String literals pushed into a vector of const char *, and given to a
// reference to a const pointer: the literal's address, not a copy's.
#include <stdio.h>
#include <vector>

static void fill(std::vector<const char *> &dst)
{
    dst.push_back("VK_KHR_sampler_mirror_clamp_to_edge");
    dst.push_back("VK_EXT_descriptor_indexing");
}

static const char *pick(const char *const &p)
{
    return p;
}

int main()
{
    std::vector<const char *> names;
    fill(names);
    char local[] = "local";
    printf("%s %s %s %s\n", names[0], names[1], pick("literal"), pick(local));
    return 0;
}
