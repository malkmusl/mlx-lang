// In a class derived (directly or not) from a class template's instance,
// the template's name alone is that base (its injected class name), in
// the class's own member declarations too: WebCore's SVGPathSegImpl.h
// `static constexpr auto create = SVGPathSegValue::create<Self>;`. The
// instance it names there, create<Self>, is made with Self still being
// read: its body is read at the end, Self complete.
#include <stdio.h>

struct Seg {
    virtual ~Seg() { }
    virtual int kind() const = 0;
};

template<typename... Arguments> class SegValue : public Seg {
public:
    static constexpr int k = 4;
    template<typename Type> static Type *create(int v)
    {
        auto *t = new Type();
        t->value = v + (int)sizeof(Type) * 0;
        return t;
    }
    int value { 0 };
};

class Horizontal : public SegValue<float> { };

class HorizontalAbs final : public Horizontal {
public:
    static constexpr auto create = SegValue::create<HorizontalAbs>;
    static constexpr int x = SegValue::k + 1;
    int kind() const final { return 7; }
};

int main(int argc, char **)
{
    Seg *s = HorizontalAbs::create(5);
    printf("%d %d %d\n", s->kind(), static_cast<HorizontalAbs *>(s)->value, HorizontalAbs::x);
    delete s;
    return argc;
}
