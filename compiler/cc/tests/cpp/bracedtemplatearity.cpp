// Overloads given a braced list of one element: a class whose
// constructor template needs two arguments is no candidate (`rect.contract(
// { sampleRectMargin })` of WebCore's LayoutRect takes a LayoutBoxExtent,
// not a LayoutSize through its `template<T, U> LayoutSize(T, U)`); one
// whose second parameter has a default is.
#include <cstdio>
struct Size {
    Size() = default;
    template<typename T, typename U> constexpr Size(T w, U h) : w(w), h(h) { }
    int w = 0, h = 0;
};
struct Edges {
    Edges(const int& v) : top(v), right(v), bottom(v), left(v) { }
    int top, right, bottom, left;
};
struct Defaulted {
    template<typename T> Defaulted(T v, int scale = 3) : v(v * scale) { }
    int v;
};
struct Rect {
    int w = 20, h = 20;
    void contract(const Size& s) { w -= s.w; h -= s.h; }
    void contract(const Edges& e) { w -= e.left + e.right; h -= e.top + e.bottom; }
};
int take(const Defaulted& d) { return d.v; }
int take(const Rect*) { return -1; }
int main() {
    static constexpr auto margin = 4;
    Rect r;
    r.contract({ margin });
    printf("%d %d\n", r.w, r.h);
    r.contract({ 1, 2 });
    printf("%d %d\n", r.w, r.h);
    printf("%d\n", take({ 5 }));
}
