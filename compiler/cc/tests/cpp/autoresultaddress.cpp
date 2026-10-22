// Functions whose result type their body says (`static auto createAnimator(...)`),
// named by address before anything calls them, while their bodies wait
// for a use: the body is read for the name's type (WebCore's
// SVGPropertyAnimatorFactory makes std::pairs of WTF::Functions of its
// `static auto createColorAnimator`).
#include <cstdio>
#include <functional>
#include <utility>

struct Prop {
    int v;
};

template<typename T> struct ValueProp {
    static Prop create() { return Prop { 7 }; }
};

inline auto twice(int x) { return x * 2; }

template<typename F> int callWith(F f, int x) { return f(x); }

struct Factory {
    static auto createAnimator(int x, Prop&& p) { return x + p.v; }
    using Pair = std::pair<std::function<Prop()>, std::function<int(int, Prop&&)>>;
    static int run()
    {
        Pair a { ValueProp<int>::create, Factory::createAnimator };
        Pair b { []() { return Prop { 3 }; }, createAnimator };
        return a.second(1, a.first()) + b.second(2, b.first());
    }
    auto member(int x) const { return x + 100; }
};

int main()
{
    printf("%d\n", Factory::run());
    auto pointer = &twice;
    decltype(&ValueProp<long>::create) creator = ValueProp<long>::create;
    printf("%d %d %d\n", pointer(4), callWith(twice, 5), creator().v);
    auto method = &Factory::member;
    Factory f;
    printf("%d\n", (f.*method)(1));
    return 0;
}
