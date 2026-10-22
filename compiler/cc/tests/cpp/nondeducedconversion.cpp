// A function template's parameter naming no template parameter deduces
// nothing: the argument converts to it, through a user-defined
// conversion too (HashMap<Node *, Ref<Breakpoint>>::add(const KeyType &,
// V &&) given a RefPtr<Node>, in WebCore's PageDOMDebuggerAgent).
#include <cstdio>
#include <utility>
template<typename T> struct RefPtr {
    RefPtr(T* p) : m_ptr(p) { }
    operator T*() const { return m_ptr; }
    explicit operator bool() const { return !!m_ptr; }
    T* m_ptr;
};
template<typename T> struct Ref {
    Ref(T& r) : m_ptr(&r) { }
    Ref(Ref&& o) : m_ptr(o.m_ptr) { }
    T* m_ptr;
};
template<typename K, typename V> class HashMap {
public:
    using KeyType = K;
    template<typename X> int add(const KeyType& k, X&& v) { key = k; value = v.m_ptr; return 1; }
    template<typename X> int add(KeyType&& k, X&& v) { key = k; value = v.m_ptr; return 2; }
    template<typename HashTranslator, typename KK, typename X> int add(KK&&, X&&) { return 3; }
    K key { };
    void* value { };
};
struct Node { int v; };
struct Breakpoint { int id; };
int f1(Node* const& k) { return k->v; }
int f2(Node*&& k) { return k->v + 10; }
template<typename X> int g1(Node* const& k, X&&) { return k->v + 20; }
template<typename X> int g2(Node*&& k, X&&) { return k->v + 30; }
int main() {
    { Node m { 5 }; RefPtr<Node> p(&m); printf("%d %d %d %d\n", f1(p), f2(p), g1(p, 1), g2(p, 1)); }
    Node n { 1 };
    Breakpoint b { 2 };
    RefPtr<Node> node(&n);
    HashMap<Node*, Ref<Breakpoint>> map;
    int r = map.add(node, Ref<Breakpoint>(b));
    printf("%d %d\n", r, map.key == &n);
}
