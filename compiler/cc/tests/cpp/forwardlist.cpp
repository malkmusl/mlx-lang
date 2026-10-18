// std::forward_list (<forward_list>), as Skia's SkSL and its GPU code use
// it: a stack of counters, owned strings whose addresses stay, removal of
// one element after a found one.
#include <forward_list>
#include <algorithm>
#include <string>
#include <cstdio>

struct Counted {
    static int alive;
    int value;
    Counted(int v) : value(v) { ++alive; }
    Counted(const Counted &other) : value(other.value) { ++alive; }
    ~Counted() { --alive; }
    bool operator==(const Counted &other) const { return value == other.value; }
};
int Counted::alive = 0;

int main(int argc, char **argv) {
    std::forward_list<int> levels{0};
    ++levels.front();
    levels.push_front(0);
    ++levels.front();
    ++levels.front();
    printf("%d %d\n", levels.front(), (int)std::count(levels.begin(), levels.end(), 1));
    levels.pop_front();
    printf("%d %d\n", levels == std::forward_list<int>{1}, levels != std::forward_list<int>{0});

    std::forward_list<std::string> owned;
    owned.push_front(std::string("first"));
    const std::string *kept = &owned.front();
    owned.emplace_front("second");
    owned.push_front("third");
    printf("%s %s\n", kept->c_str(), owned.front().c_str());

    {
        std::forward_list<Counted> list;
        for (int i = 4; i >= 1; --i) list.emplace_front(i);
        Counted two(2);
        auto prev = list.before_begin();
        for (auto cur = list.begin(); cur != list.end(); prev = cur++) {
            if (*cur == two) {
                list.erase_after(prev);
                break;
            }
        }
        list.remove(Counted(4));
        list.reverse();
        std::forward_list<Counted> copy = list;
        for (const Counted &c : copy) printf("%d ", c.value);
        printf("| %d\n", Counted::alive);
        list.clear();
        printf("%d %d\n", list.empty(), Counted::alive);
    }
    printf("%d\n", Counted::alive);
    return argc;
}
