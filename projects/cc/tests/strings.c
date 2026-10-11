/* String and character literals. */
#include <stdio.h>
#include <string.h>
#include <stddef.h>

int main(void) {
    const char *s = "tab\there\nnew \"quote\" \\ \x41\102\0hidden";
    printf("%s|%d\n", s, (int)sizeof("tab\there"));
    printf("%s\n", "con" "cat" "enated");
    char a[] = "abc";
    char b[10] = "xy";
    char c[3] = "abc";
    printf("%d %d %d %c%c%c\n", (int)sizeof a, (int)sizeof b, b[5], c[0], c[1], c[2]);
    wchar_t w[] = L"wide";
    printf("%d %d %d\n", (int)sizeof w, w[0], w[4]);
    unsigned short u16[] = u"été";
    unsigned int u32[] = U"\U0001F600!";
    printf("%d %x %d %x\n", (int)sizeof u16, u16[0], (int)sizeof u32, u32[0]);
    const char *utf8 = u8"café";
    printf("%d %s\n", (int)strlen(utf8), utf8);
    printf("%d %d %d %d\n", 'A', '\n', '\377', L'\xff');
    char multi[] = { "braced" };
    printf("%s\n", multi);
    const char *table[] = { "first", "second" "third" };
    printf("%s %s\n", table[0], table[1]);
    printf("%c\n", "index"[2]);
    printf("%d\n", "abc" != NULL);
    return 0;
}
