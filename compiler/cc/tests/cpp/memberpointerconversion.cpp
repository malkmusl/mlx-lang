// A conversion to a pointer to member converts to bool and to nothing
// else: WTF's String declares two (never defined, so that no code turns
// a String into a bool), and StringView's endsWith(char16_t) was taken
// for endsWith(StringView) with a String argument, through one of them
// and a char16_t. The safe-bool idiom still makes a condition.
#include <cstdio>

struct Str {
    const char* p;
    typedef struct DisallowedA* (Str::*BoolA);
    typedef struct DisallowedB* (Str::*BoolB);
    operator BoolA() const;
    operator BoolB() const;
};
struct View {
    const char* p;
    View(const Str& s) : p(s.p) { }
    bool endsWith(char16_t) const { return false; }
    bool endsWith(View) const { return true; }
    int contains(char16_t) const { return 1; }
    int contains(View) const { return 2; }
};

struct Safe {
    int value;
    typedef int Safe::*SafeBool;
    operator SafeBool() const { return value ? &Safe::value : nullptr; }
};

int main()
{
    Str s { "x" };
    View v { s };
    std::printf("%d %d\n", v.endsWith(s), v.contains(s));
    Safe yes { 3 };
    Safe no { 0 };
    if (yes)
        std::printf("yes\n");
    if (!no)
        std::printf("not no\n");
    bool b = yes;
    std::printf("%d\n", b);
    return 0;
}
