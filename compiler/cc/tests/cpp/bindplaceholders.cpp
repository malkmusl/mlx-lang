// std::bind: arguments bound, `placeholders::_1`... taking the call's, a
// reference_wrapper's object passed as a reference, a member function
// bound to its object (WTF's RunLoop::Timer, WebCore's
// MarkupAccumulator `std::bind(isElementExcludedByRule, _1, std::ref(element))`).
#include <algorithm>
#include <functional>
#include <stdio.h>
#include <string>
#include <vector>

struct Element { std::string name; int seen = 0; };

static bool excluded(const std::string &rule, Element &element)
{
    ++element.seen;
    return rule == element.name;
}

struct Counter {
    int total = 0;
    void add(int n) { total += n; }
    int scaled(int a, int b) const { return (total + a) * b; }
};

static int subtract(int a, int b) { return a - b; }

int main(int argc, char **)
{
    Element element { "div" };
    std::vector<std::string> rules { "span", "div", "p" };
    bool any = std::any_of(rules.begin(), rules.end(), std::bind(excluded, std::placeholders::_1, std::ref(element)));
    auto minus = std::bind(subtract, std::placeholders::_2, std::placeholders::_1);
    auto tenMinus = std::bind(subtract, 10, std::placeholders::_1);
    Counter counter;
    auto add = std::bind(&Counter::add, &counter, std::placeholders::_1);
    add(5);
    add(7);
    auto scaled = std::bind(&Counter::scaled, std::cref(counter), 1, std::placeholders::_1);
    printf("%d %d %d %d %d %d\n", (int)any, element.seen, minus(3, 10), tenMinus(4), counter.total, scaled(2));
    printf("%d %d\n", (int)std::is_bind_expression_v<decltype(minus)>, std::is_placeholder_v<decltype(std::placeholders::_2)>);
    return argc;
}
