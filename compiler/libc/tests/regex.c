/* POSIX regular expressions (regex.h): each case compiled with regcomp
 * and run with regexec, printing the codes, regerror's message, re_nsub
 * and the groups' offsets, so that the output of a build against glibc and
 * of one against mlxlibc can be compared line by line
 * (tools/check_regex.sh). */
#include <regex.h>
#include <stdio.h>
#include <string.h>

struct test {
    int cflags;
    const char *pattern;
    int eflags;
    const char *text;
};

#define E REG_EXTENDED
#define I REG_ICASE
#define N REG_NEWLINE
#define S REG_NOSUB

static const struct test tests[] = {
    /* Extended expressions: characters, alternatives, repetitions. */
    { E, "abc", 0, "xxabcxx" },
    { E, "a|b|c", 0, "zzc" },
    { E, "a*", 0, "aaab" },
    { E, "a+", 0, "baaab" },
    { E, "ba?b", 0, "abbab" },
    { E, "x(a|ab)(c|bcd)(d*)", 0, "xabcd" },
    { E, "(a|ab)(bc|c)", 0, "abc" },
    { E, "(a*)*", 0, "b" },
    { E, "(a*)+", 0, "aab" },
    { E, "(a|b)*c", 0, "ababc" },
    { E, "(a*)*b", 0, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaac" },
    { E, "((a|b)*)*(c|d)+e", 0, "abababababababababababababccddcdx abcde" },
    { E, "(a|aa)*", 0, "aaaaa" },
    { E, "([a-z]+)@([a-z]+)\\.com", 0, "mail me: joe@example.com." },
    { E, "(.*)-(.*)", 0, "one-two-three" },
    { E, "a{2,3}", 0, "aaaa" },
    { E, "a{2}", 0, "a aa" },
    { E, "a{2,}", 0, "aaaaa" },
    { E, "a{,2}b", 0, "aaab" },
    { E, "(ab){1,2}c", 0, "abababc" },
    { E, "^abc$", 0, "abc" },
    { E, "^abc$", 0, "abcd" },
    { E, "b$", 0, "ab\nb" },
    { E | N, "^b", 0, "a\nb" },
    { E | N, "a.c", 0, "a\nc abc" },
    { E | N, "a[^x]c", 0, "a\nc" },
    { E, "a[^x]c", 0, "a\nc" },
    { E, "^a", 0, "a" },
    { E, "x)", 0, "x)" },
    { E, "()", 0, "abc" },
    { E, "(a)|b", 0, "b" },
    { E, "(a)(b)?", 0, "a" },
    { E, "", 0, "abc" },
    { E, "a||b", 0, "b" },
    /* Bracket expressions. */
    { E, "[abc]+", 0, "xxbcay" },
    { E, "[^abc]+", 0, "abcxyzab" },
    { E, "[]a]+", 0, "x]a]" },
    { E, "[^]a]+", 0, "]ab" },
    { E, "[a-]+", 0, "x-a-" },
    { E, "[[:digit:]]+", 0, "abc123def" },
    { E, "[[:alpha:][:digit:]]+", 0, "--ab12--" },
    { E, "[[:space:]]+", 0, "a \t\nb" },
    { E, "[[:upper:]]+", 0, "abCDe" },
    { E, "[[:upper:]]+", I, "abCDe" },
    { E, "[[:punct:]]+", 0, "ab!?.c" },
    { E, "[[:xdigit:]]+", 0, "xyzBEEFg" },
    { E, "[[.a.]-c]+", 0, "xabcd" },
    { E, "[[=a=]]+", 0, "baab" },
    { E, "[a-c-e]", 0, "d" },
    /* Case. */
    { E | I, "HeLLo", 0, "say hello" },
    { E | I, "[a-c]+", 0, "xABCy" },
    { E | I, "(a)\\1", 0, "aA" },
    /* Back-references. */
    { E, "(a+)b\\1", 0, "aabaa" },
    { E, "(.)\\1", 0, "abccd" },
    { 0, "\\(a*\\)b\\1", 0, "aaabaaa" },
    { 0, "\\(a\\)\\(b\\)\\2\\1", 0, "xabba" },
    { E, "(a)|\\1", 0, "x" },
    /* GNU's operators. */
    { E, "\\w+", 0, "  foo_1 " },
    { E, "\\W+", 0, "foo  bar" },
    { E, "\\s+", 0, "a \t b" },
    { E, "\\bfoo\\b", 0, "afoo foo" },
    { E, "\\Boo", 0, "foo" },
    { E, "\\<b", 0, "ab b" },
    { E, "a\\>", 0, "ab a" },
    { E, "\\`a", 0, "ba" },
    { E, "a\\'", 0, "aa" },
    /* Basic expressions. */
    { 0, "a*b", 0, "aab" },
    { 0, "*a", 0, "x*a" },
    { 0, "^*a", 0, "*a" },
    { 0, "a\\{2\\}", 0, "aaa" },
    { 0, "\\(ab\\)*c", 0, "ababc" },
    { 0, "a|b", 0, "a|b" },
    { 0, "a\\|b", 0, "b" },
    { 0, "a+", 0, "a+" },
    { 0, "a\\+", 0, "aaa" },
    { 0, "a\\?b", 0, "b" },
    { 0, "a^b", 0, "a^b" },
    { 0, "a$b", 0, "a$b" },
    { 0, "\\(^a\\)", 0, "a" },
    { 0, "\\(a$\\)", 0, "a" },
    { 0, "x(a)", 0, "x(a)" },
    { 0, "a{1}", 0, "a{1}" },
    /* Execution flags. */
    { E, "^a", REG_NOTBOL, "a" },
    { E, "a$", REG_NOTEOL, "a" },
    { E | N, "^a", REG_NOTBOL, "b\na" },
    { E | S, "(a)(b)", 0, "ab" },
    /* Errors. */
    { E, "(", 0, "" },
    { E, "a(b", 0, "" },
    { 0, "\\(", 0, "" },
    { 0, "a\\)", 0, "" },
    { E, "[a", 0, "" },
    { E, "[[:nope:]]", 0, "" },
    { E, "[z-a]", 0, "" },
    { E, "a\\", 0, "" },
    { E, "*a", 0, "" },
    { E, "a|*b", 0, "" },
    { E, "a{3,2}", 0, "" },
    { E, "a{1", 0, "" },
    { E, "\\1", 0, "" },
    { E, "(a\\1)", 0, "" },
    { E, "[[.ab.]]", 0, "" },
    { 0, "a\\{1", 0, "" },
};

int main(void)
{
    char message[64];
    for (size_t i = 0; i < sizeof tests / sizeof tests[0]; i++) {
        const struct test *t = &tests[i];
        regex_t re;
        int code = regcomp(&re, t->pattern, t->cflags);
        printf("%zu /%s/ %d:", i, t->pattern, t->cflags);
        if (code != 0) {
            size_t size = regerror(code, &re, message, sizeof message);
            printf(" regcomp %d \"%s\" (%zu)\n", code, message, size);
            continue;
        }
        printf(" nsub %zu", re.re_nsub);
        regmatch_t match[10];
        memset(match, 0x55, sizeof match);
        code = regexec(&re, t->text, 10, match, t->eflags);
        if (code != 0) {
            printf(" regexec %d\n", code);
        } else {
            size_t shown = (t->cflags & REG_NOSUB) ? 0 : re.re_nsub + 1;
            for (size_t g = 0; g < shown; g++)
                printf(" (%d,%d)", (int) match[g].rm_so, (int) match[g].rm_eo);
            printf("\n");
        }
        regfree(&re);
    }
    /* A short buffer for regerror; REG_STARTEND. */
    size_t size = regerror(REG_EBRACK, NULL, message, 8);
    printf("regerror short: \"%s\" %zu\n", message, size);
    regex_t re;
    if (regcomp(&re, "b+", REG_EXTENDED) == 0) {
        regmatch_t match[1] = { { 2, 5 } };
        int code = regexec(&re, "bbabbbb", 1, match, REG_STARTEND);
        printf("startend %d (%d,%d)\n", code, (int) match[0].rm_so, (int) match[0].rm_eo);
        regfree(&re);
    }
    return 0;
}
