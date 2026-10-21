// std::optional<T> assigned an object that is no T but converts to
// std::optional<T> (WTF's Markable<WallTime> in WebCore's
// Document::lastModified): through the conversion, not the assignment
// from a value T is made of.
#include <cstdio>
#include <optional>

struct Time {
    double seconds = 0;
};

struct MaybeTime {
    Time value;
    bool present = false;
    explicit operator bool() const { return present; }
    operator std::optional<Time>() const { return present ? std::optional<Time>(value) : std::nullopt; }
};

int main() {
    MaybeTime m;
    m.value.seconds = 12.5;
    m.present = true;
    std::optional<Time> t;
    if (m) t = m;
    printf("%d %.1f\n", t.has_value(), t ? t->seconds : -1.0);
    MaybeTime none;
    t = none;
    printf("%d\n", t.has_value());
    return 0;
}
