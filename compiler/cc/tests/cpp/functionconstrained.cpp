// std::function<R(Args...)> is made only from a callable taking Args
// (giving what converts to R): of another, is_convertible is false and
// an overload constrained on it is taken (WebCore's
// ContentSecurityPolicy::allPoliciesWithDispositionAllow with a pointer
// to a member function of another class: "too few arguments").
#include <stdio.h>
#include <functional>
#include <string>
#include <type_traits>

struct Directive { int id; };
struct List {
    int base = 10;
    const Directive *find(const std::string &) const { return nullptr; }
    int value(const Directive &d) const { return base + d.id; }
};
using Callback = std::function<void(const Directive &)>;

template<typename Predicate> requires (!std::is_convertible_v<Predicate, Callback>) int allow(Predicate &&) { return 1; }
template<typename Predicate> int allow(Callback &&, Predicate &&) { return 2; }

int main(int argc, char **argv)
{
    printf("%d %d\n", (int)std::is_convertible_v<decltype(&List::find), Callback>, (int)std::is_convertible_v<void (*)(const Directive &), Callback>);
    printf("%d %d\n", allow(&List::find), allow([](const Directive &) { }, &List::find));
    List list;
    std::function<int(const List &, const Directive &)> member = &List::value;
    std::function<long(const Directive &)> lambda = [](const Directive &d) { return d.id * 2; };
    Callback ignoring = [](const Directive &d) { return d.id; };
    ignoring(Directive { 1 });
    printf("%d %ld\n", member(list, Directive { 5 }), lambda(Directive { 4 }));
    return argc;
}
