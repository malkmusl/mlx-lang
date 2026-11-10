// The C++ reference of the string_utils differential test: the same
// calls as string_utils_test.mlx, one line per call.
#include "common/string_utils.h"
#include <cstdio>
#include <string>
#include <string_view>
#include <vector>
namespace gl { namespace priv { std::ostream *gSwallowStream = nullptr; } }
// string_utils.cpp reads the environment through system_utils.cpp; this test
// asks nothing of the environment.
namespace angle { std::string GetEnvironmentVarOrAndroidProperty(const char *, const char *) { return std::string(); } }
using namespace angle;
static void text(const std::string &value) { printf("[%.*s]", (int)value.size(), value.data()); }
static void view(std::string_view value) { printf("[%.*s]", (int)value.size(), value.data()); }
static void number(unsigned long long v) { printf("%llu ", v); }
static void flag(bool v) { number(v ? 1 : 0); }
static void line() { printf("\n"); }
static void splits(const char *input, const char *delimiters) {
    std::string in(input);
    std::string_view delims(delimiters);
    for (int whitespace = 0; whitespace < 2; whitespace++) {
        for (int want = 0; want < 2; want++) {
            std::vector<std::string> parts = SplitString(in, delims, (WhitespaceHandling)whitespace, (SplitResult)want);
            number(parts.size());
            for (size_t k = 0; k < parts.size(); k++) text(parts[k]);
            line();
            std::vector<std::string_view> views = SplitStringView(std::string_view(input), delims, (WhitespaceHandling)whitespace, (SplitResult)want);
            number(views.size());
            for (size_t k = 0; k < views.size(); k++) view(views[k]);
            line();
        }
    }
}
static void trims(const char *input, const char *chars) {
    std::string in(input), trimChars(chars);
    text(TrimString(in, trimChars));
    view(TrimStringView(std::string_view(input), trimChars));
    line();
}
static void prefixes(const char *input, size_t offset, const char *delimiter, char byte) {
    std::string in(input);
    text(GetPrefix(in, offset, delimiter));
    text(GetPrefix(in, offset, byte));
    line();
}
static void edges(const char *str, const char *other, size_t length) {
    std::string a(str), b(other);
    flag(BeginsWith(a, b));
    flag(BeginsWith(a, other));
    flag(BeginsWith(str, other));
    flag(BeginsWith(a, b, length));
    flag(EndsWith(a, b));
    flag(EndsWith(a, other));
    flag(EndsWith(str, other));
    line();
}
static void tokens(const char *list, char delimiter, const char *token) {
    std::string a(list), b(token);
    flag(ContainsToken(a, delimiter, b));
    line();
}
static void cases(const char *input) {
    std::string lower(input), upper(input), camel(input);
    ToLower(&lower);
    ToUpper(&upper);
    text(lower);
    text(upper);
    text(ToCamelCase(camel));
    line();
}
static void replacements(const char *input, const char *from, const char *to) {
    std::string once(input), all(input), sub(from), repl(to);
    flag(ReplaceSubstring(&once, sub, repl));
    text(once);
    number((unsigned)ReplaceAllSubstrings(&all, sub, repl));
    text(all);
    line();
}
static void wildcards(const char *glob, const char *name) {
    flag(NamesMatchWithWildcard(std::string_view(glob), std::string_view(name)));
    line();
}
int main() {
    splits("a,b,,c", ",");
    splits("  alpha beta\tgamma  ", " \t");
    splits("one;two; three ;;", "; ");
    splits("", ",");
    splits(",,,", ",");
    splits("no delimiters here", ",");
    trims("  padded  ", " ");
    trims("xxhelloxx", "x");
    trims("    ", " ");
    trims("", " ");
    trims("\t\n mixed \r\n", " \t\r\n");
    prefixes("GL_ARB_texture_float", 0, "_", '_');
    prefixes("GL_ARB_texture_float", 3, "_", '_');
    prefixes("GL_ARB_texture_float", 7, "zz", 'z');
    prefixes("key=value;rest", 4, ";", ';');
    edges("prefix_body_suffix", "prefix", 3);
    edges("prefix_body_suffix", "suffix", 6);
    edges("short", "much longer than the string", 2);
    edges("same", "same", 4);
    edges("", "", 0);
    edges("abc", "", 0);
    tokens("GL_EXT_a GL_EXT_b GL_EXT_c", ' ', "GL_EXT_b");
    tokens("GL_EXT_a GL_EXT_b GL_EXT_c", ' ', "GL_EXT_bc");
    tokens("GL_EXT_a GL_EXT_b GL_EXT_c", ' ', "GL_EXT_c");
    tokens("a,b,c", ',', "");
    tokens("a,b,c", ',', "a");
    tokens("abc", ',', "abc");
    cases("Mixed_Case_name_WITH_123");
    cases("already_camel");
    cases("__leading_and_trailing__");
    cases("");
    replacements("the cat sat on the mat", "the", "a");
    replacements("aaaa", "aa", "b");
    replacements("nothing to do", "xyz", "q");
    replacements("abab", "ab", "");
    wildcards("GL_*", "GL_ARB_texture");
    wildcards("GL_*", "EGL_KHR_image");
    wildcards("*", "");
    wildcards("GL_?_ext", "GL_A_ext");
    wildcards("exact", "exact");
    wildcards("exact", "exactly");
    wildcards("a*b*c", "axxbyyc");
    wildcards("a*b*c", "axxbyy");
    return 0;
}
