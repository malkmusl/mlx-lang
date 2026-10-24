// An empty pack given explicitly (`f<Tags...>` with no Tags) to a
// name whose templates differ: `template<void* = nullptr>` takes it,
// `template<typename Tag, typename... Others>` cannot (Tag bound to
// the pack's holder made mlx-cc read `Tag::tag` of it and recurse
// without end mangling it; WebCore's HTMLFastPathParser).
#include <cstdio>
struct Li { static constexpr int tag = 1; };
struct Option { static constexpr int tag = 2; };
struct Parser {
    template<typename... Tags> int parseSpecific(int name)
    {
        return parseSpecific<Tags...>(name, 0);
    }
    template<void* = nullptr> int parseSpecific(int, int) { return -1; }
    template<typename Tag, typename... OtherTags> int parseSpecific(int name, int depth)
    {
        if (name == Tag::tag)
            return depth;
        return parseSpecific<OtherTags...>(name, depth + 1);
    }
};
int main() {
    Parser p;
    std::printf("%d %d %d\n", p.parseSpecific<Li, Option>(2), p.parseSpecific<Li>(1), p.parseSpecific<Option>(5));
    return 0;
}
