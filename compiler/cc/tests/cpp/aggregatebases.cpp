// C++17's aggregates with base classes: the braces' elements make the
// bases first, then the members (WebCore's ExposedColorType<C, Model>, an
// aggregate deriving from the color type, made of a color by a derived
// class's member initializer).
#include <cstdio>

struct Color { int r; int g; };
struct Plain : Color { };
struct More : Color { int extra; };
struct Two : Plain, More { int last; };
struct Defaulted : Color { int x = 42; };

struct Model { };
template<typename C, typename M = typename C::Model> struct Exposed;
template<typename C> struct Exposed<C, Model> : C {
    using C::r;
    using C::g;
};

struct RGB { using Model = ::Model; int r; int g; };

template<typename C> struct Resolved : Exposed<C, typename C::Model> {
    explicit constexpr Resolved(C color) : Exposed<C, typename C::Model> { resolve(color) } { }
    static constexpr C resolve(C c) { return C { c.r + 1, c.g + 1 }; }
};

static int sum(const Plain& p) { return p.r + p.g; }

int main()
{
    Plain p { Color { 7, 8 } };
    Plain q { { 9, 10 } };
    More m { { 1, 2 }, 3 };
    Color c { 11, 12 };
    More n { c, 4 };
    Two t { c, { { 5, 6 }, 7 }, 8 };
    Defaulted d { { 1, 1 } };
    Defaulted e { };
    std::printf("%d %d %d %d\n", p.r, p.g, q.r, q.g);
    std::printf("%d %d %d %d %d\n", m.r, m.g, m.extra, n.r, n.extra);
    std::printf("%d %d %d %d\n", static_cast<Color&>(static_cast<More&>(t)).r, t.More::g, t.More::extra, t.last);
    std::printf("%d %d %d %d\n", d.r, d.x, e.r, e.x);
    Resolved<RGB> x { RGB { 1, 2 } };
    Exposed<RGB> exposed { RGB { 5, 6 } };
    std::printf("%d %d %d %d\n", x.r, x.g, exposed.r, exposed.g);
    std::printf("%d\n", sum(Plain { { 20, 22 } }));
    return t.last;
}
