// A comparison defaulted outside its class, `bool C::operator==(const C
// &) const = default;` (WebCore's CSSCalc::Child), compares member by
// member as one defaulted in the class does.
#include <stdio.h>
#include <string>

struct Child {
    int kind;
    std::string name;
    double weight;
    bool operator==(const Child &) const;
    bool operator!=(const Child &other) const;
};

bool Child::operator==(const Child &) const = default;
bool Child::operator!=(const Child &other) const = default;

struct Pair {
    Child first;
    long count;
    bool operator==(const Pair &) const;
};

bool Pair::operator==(const Pair &) const = default;

int main(int argc, char **)
{
    Child a { 1, "x", 1.5 };
    Child b { 1, "x", 1.5 };
    Child c { 1, "y", 1.5 };
    Pair p { a, 3 };
    Pair q { b, 3 };
    Pair r { c, 3 };
    printf("%d %d %d %d %d\n", (int)(a == b), (int)(a == c), (int)(a != c), (int)(p == q), (int)(p == r));
    return argc;
}
