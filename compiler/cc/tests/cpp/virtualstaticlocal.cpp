// A static local initialized by a virtual call (WebCore's Scrollbar:
// `static int maxOverlapBetweenPages =
// ScrollbarTheme::theme().maxOverlapBetweenPages();`): a call through
// the vtable, not a constant to compute, made when the function first
// runs.
#include <cstdio>
#include <limits>

struct Theme {
    static Theme& theme();
    virtual int maxOverlap() { return std::numeric_limits<int>::max(); }
};

struct Native : Theme {
    int maxOverlap() override { return 40; }
};

Theme& Theme::theme()
{
    static Native native;
    return native;
}

int overlap()
{
    static int maxOverlap = Theme::theme().maxOverlap();
    return maxOverlap;
}

int main()
{
    printf("%d\n", overlap());
    return 0;
}
