// A ranges algorithm's projection that returns a reference: the
// comparison gets the object itself, never a copy (WebCore's
// KeyframeEffectStack::sortedEffects sorts its effects by
// `*effect->animation()`, a WebAnimation & whose copy was destroyed while
// the animation was referenced: RefCountedBase's assertion).
#include <algorithm>
#include <stdio.h>
#include <vector>

static int copies;
struct Animation {
    int order;
    explicit Animation(int o) : order(o) { }
    Animation(const Animation& other) : order(other.order) { ++copies; }
};
struct Effect { Animation* animation; };

static bool byOrder(const Animation& a, const Animation& b) { return a.order < b.order; }

int main()
{
    Animation a { 3 }, b { 1 }, c { 2 };
    std::vector<Effect> effects { { &a }, { &b }, { &c } };
    auto project = [](auto& effect) -> Animation& { return *effect.animation; };
    std::ranges::stable_sort(effects, byOrder, project);
    for (auto& effect : effects)
        printf("%d ", effect.animation->order);
    std::ranges::sort(effects, [](const Animation& x, const Animation& y) { return x.order > y.order; }, project);
    for (auto& effect : effects)
        printf("%d ", effect.animation->order);
    printf("\ncopies %d sorted %d\n", copies, (int)std::ranges::is_sorted(effects, [](const Animation& x, const Animation& y) { return x.order > y.order; }, project));
    return 0;
}
