// std::variant (alternatives chosen by type, copied, emplaced, destroyed;
// get, get_if, holds_alternative, visit of one and of two variants) and
// a declaration as an if condition.
#include <stdio.h>
#include <variant>
#include <string>
struct Tracked {
    static int live;
    int id;
    Tracked(int i) : id(i) { live++; }
    Tracked(const Tracked &o) : id(o.id) { live++; }
    ~Tracked() { live--; }
};
int Tracked::live = 0;
struct Printer {
    void operator()(int v) const { printf("int %d\n", v); }
    void operator()(double v) const { printf("double %g\n", v); }
    void operator()(const std::string &v) const { printf("string %s\n", v.c_str()); }
};
struct Sum {
    int operator()(int a, int b) const { return a + b; }
    int operator()(int a, char b) const { return a * 100 + b; }
    int operator()(char a, int b) const { return a - b; }
    int operator()(char a, char b) const { return a * b; }
};
int main() {
    std::variant<int, double, std::string> v;
    printf("%d %d\n", (int)v.index(), std::get<int>(v));
    v = 2.5;
    printf("%d %g %d\n", (int)v.index(), std::get<double>(v), (int)std::holds_alternative<double>(v));
    v = std::string("hello");
    printf("%d %s\n", (int)v.index(), std::get<2>(v).c_str());
    std::visit(Printer(), v);
    v = 7;
    std::visit(Printer(), v);
    if (int *p = std::get_if<int>(&v)) printf("get_if %d\n", *p);
    printf("%d\n", std::get_if<double>(&v) == nullptr);
    std::variant<int, double, std::string> w = v;
    printf("copy %d %d %d\n", (int)w.index(), std::get<int>(w), (int)(w == v));
    {
        std::variant<std::monostate, Tracked> t;
        printf("mono %d live %d\n", (int)t.index(), Tracked::live);
        t.emplace<Tracked>(5);
        printf("emplaced %d live %d id %d\n", (int)t.index(), Tracked::live, std::get<Tracked>(t).id);
        std::variant<std::monostate, Tracked> u = t;
        printf("copied live %d\n", Tracked::live);
        t = std::monostate();
        printf("reset live %d\n", Tracked::live);
    }
    printf("after live %d\n", Tracked::live);
    std::variant<int, char> a = 3, b = 'x';
    printf("%d %d %d\n", std::visit(Sum(), a, a), std::visit(Sum(), a, b), std::visit(Sum(), b, a));
    printf("%d %d\n", (int)std::variant_size<std::variant<int, char, long> >::value, (int)sizeof(std::variant_alternative_t<2, std::variant<int, char, long> >));
    return 0;
}
