// The standard library's containers made of braced lists, counts,
// allocators and initializer_lists, in the forms code writes them: a
// member `v({ p })` takes vector's `vector(initializer_list<T>, const
// Alloc & = Alloc())` (not `explicit vector(const Alloc &)`, whose
// allocator a pointer makes nothing of: Skia's SymbolRenamer), `v{ p }`,
// `vector<T *> v(n)`, `map{ { k, v } }`, lists as arguments, returns and
// assignments.
#include <array>
#include <cstdio>
#include <deque>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

struct Table {
    int id;
};

struct Stack {
    Stack(Table* base) : tables({ base }), more{ base, base } { }
    std::vector<Table*> tables;
    std::vector<Table*> more;
};

struct Named {
    Named(const char* a, const char* b) : names({ a, b }), byName({ { a, 1 }, { b, 2 } }) { }
    std::vector<std::string> names;
    std::map<std::string, int> byName;
};

static int total(const std::vector<Table*>& v)
{
    int sum = 0;
    for (Table* t : v)
        sum += t ? t->id : 100;
    return sum;
}

static int sizes(const std::map<int, std::vector<int>>& m)
{
    int n = 0;
    for (auto& entry : m)
        n += entry.first * (int)entry.second.size();
    return n;
}

static std::vector<Table*> pair(Table* a, Table* b) { return { a, b }; }
static std::vector<int> counted() { return std::vector<int>(3, 7); }

int main()
{
    Table one { 1 }, two { 2 }, three { 3 };

    Stack s(&one);
    printf("%zu %zu %d %d\n", s.tables.size(), s.more.size(), total(s.tables), total(s.more));

    std::vector<Table*> a { &one, &two };
    std::vector<Table*> b({ &three });
    std::vector<Table*> c(4);
    std::vector<Table*> d(2, &two);
    std::vector<Table*> e = { &one, &two, &three };
    std::vector<Table*> f(e.begin() + 1, e.end());
    std::vector<Table*> g { nullptr };
    std::allocator<Table*> alloc;
    std::vector<Table*> h(alloc);
    std::vector<Table*> i({ &one, &two }, alloc);
    printf("%zu %zu %zu %zu %zu %zu %zu %zu %zu\n", a.size(), b.size(), c.size(), d.size(), e.size(), f.size(), g.size(), h.size(), i.size());
    printf("%d %d %d %d %d\n", total(a), total(b), total(d), total(f), total(i));
    printf("%d %d\n", total({ &two, &three }), total(pair(&one, &three)));

    a = { &three };
    a.push_back({ &two });
    a.insert(a.begin(), { &one, &one });
    printf("%zu %d\n", a.size(), total(a));

    std::vector<int> n1(3), n2 { 3 }, n3(3, 7), n4 { 3, 7 }, n5 = counted();
    printf("%zu %zu %zu %zu %zu %d\n", n1.size(), n2.size(), n3.size(), n4.size(), n5.size(), n4[1]);

    std::vector<std::vector<int>> nested { { 1, 2 }, { 3 }, { } };
    std::vector<std::pair<int, std::string>> pairs { { 1, "one" }, { 2, "two" } };
    printf("%zu %zu %zu %s\n", nested.size(), nested[0].size(), pairs.size(), pairs[1].second.c_str());

    std::map<int, const char*> m { { 1, "a" }, { 2, "b" } };
    std::map<std::string, int> m2({ { "x", 10 }, { "y", 20 } });
    std::map<int, std::vector<int>> m3 { { 1, { 1, 2, 3 } }, { 2, { } } };
    m3.insert({ 3, { 4 } });
    m[3] = "c";
    printf("%zu %s %d %d %d\n", m.size(), m[2], m2["y"], sizes(m3), sizes({ { 5, { 1, 1 } } }));

    std::set<Table*> ptrs { &one, &two, &one };
    std::set<int> ints({ 3, 1, 2, 3 });
    std::unordered_map<std::string, Table*> um { { "one", &one }, { "two", &two } };
    printf("%zu %zu %d %d\n", ptrs.size(), ints.size(), *ints.begin(), um["two"]->id);

    std::deque<Table*> dq { &one, &two };
    std::list<int> li { 4, 5, 6 };
    std::string str(3, 'x'), str2 { 'a', 'b' };
    printf("%zu %zu %s %s\n", dq.size(), li.size(), str.c_str(), str2.c_str());

    Named named("p", "q");
    printf("%zu %d\n", named.names.size(), named.byName["q"]);

    std::vector<std::unique_ptr<Table>> owned;
    owned.push_back(std::make_unique<Table>(Table { 9 }));
    owned.emplace_back(new Table { 8 });
    printf("%zu %d\n", owned.size(), owned[0]->id + owned[1]->id);

    std::array<Table*, 2> arr { &one, &two };
    std::array<std::pair<int, int>, 2> arrPairs { { { 1, 2 }, { 3, 4 } } };
    std::vector<Table> tables { { 1 }, { 2 }, { 3 } };
    std::vector<Table> moreTables(2, Table { 5 });
    std::optional<std::vector<int>> maybe({ 1, 2 });
    std::pair<int, std::vector<int>> withList { 1, { 2, 3 } };
    std::tuple<int, std::string, Table*> tuple { 1, "t", &three };
    std::vector<std::function<int(int)>> calls { [](int x) { return x + 1; }, [](int x) { return x * 2; } };
    printf("%d %d %zu %d %zu %zu %d %s %d %d\n", arr[1]->id, arrPairs[1].second, tables.size(), moreTables[1].id, maybe->size(), withList.second.size(), std::get<0>(tuple), std::get<1>(tuple).c_str(), std::get<2>(tuple)->id, calls[0](3) + calls[1](3));
    return 0;
}
