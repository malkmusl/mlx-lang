// A class template's virtual member defined outside the class is made
// at the end of the unit, its point of instantiation: the class deriving
// from the instance (CRTP) is complete by then (WebCore's
// CachedHTMLCollection<TagCollection>::namedItem calling TagCollection's
// elementMatches: "incomplete struct or union"). A virtual destructor
// so defined too.
#include <stdio.h>

struct Base {
    virtual int named(int) const = 0;
    virtual ~Base() { printf("~Base\n"); }
};
template<typename C> struct Cached : Base {
    const C &collection() const { return static_cast<const C &>(*this); }
    int named(int x) const override;
    ~Cached() override;
};
template<typename C> int Cached<C>::named(int x) const
{
    if (collection().matches(x))
        return collection().value + x;
    return -1;
}
template<typename C> Cached<C>::~Cached()
{
    printf("~Cached %d\n", collection().value);
}
struct Tags final : Cached<Tags> {
    int value = 40;
    bool matches(int x) const { return x > 0; }
};
struct Names final : Cached<Names> {
    int value = 7;
    bool matches(int x) const { return x < 0; }
};

int main(int argc, char **argv)
{
    Base *b = new Tags;
    Names n;
    const Base &r = n;
    printf("%d %d %d %d\n", b->named(2), b->named(-1), r.named(2), r.named(-3));
    delete b;
    return argc;
}
