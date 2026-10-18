// Range-for over iterators with a defaulted friend operator== only: the loop's test is the rewritten !(it == end)
#include <stdio.h>

namespace p_defeq {
struct Node { Node *n; Node *next() const { return n; } };
template <typename Raw, typename T> class It {
public:
    explicit It(Raw *node) : m_node(node) {}
    T &operator*() const { return static_cast<T &>(*m_node); }
    It &operator++() { m_node = m_node->next(); return *this; }
    friend bool operator==(It, It) = default;
private:
    Raw *m_node;
};
struct List {
    Node s;
    List() { s.n = &s; }
    It<Node, Node> begin() { return It<Node, Node>(s.next()); }
    It<Node, Node> end() { return It<Node, Node>(&s); }
};
struct P { int a; long b; friend bool operator==(const P &, const P &) = default; bool operator!=(int) const { return false; } };
int run() {
    List l;
    Node a, b;
    a.n = &b; b.n = &l.s; l.s.n = &a;
    int count = 0;
    for (auto &x : l) { (void)x; if (++count > 10) break; }
    It<Node, Node> i(&a), j(&a), k(&b);
    P p{1, 2}, q{1, 2}, r{1, 3};
    printf("%d %d %d %d %d %d\n", count, i == j, i != k, p == q, p == r, !(p != q));
    return 0;
}
}

int main() {
    p_defeq::run();
    return 0;
}
