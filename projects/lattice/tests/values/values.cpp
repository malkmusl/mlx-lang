// The values test: classes handed over by value and base-class
// subobjects. A class with a copy constructor taken by value (C++ passes
// the address of a copy), an empty base copied and returned as a
// reference, a base assigned through the derived object, a reference a
// virtual call returns.
namespace values {
struct Index {
    int value;
    Index() : value(0) {}
    Index(int v) : value(v) {}
    Index(const Index &other) : value(other.value + 1) {}
};
struct Empty {
    int twice(int v) const { return v * 2; }
};
template <class D> struct Box : D {
    int payload;
    Box(const D &d, int p) : D(d), payload(p) {}
    D &deleter() { return *this; }
};
struct Base {
    int a;
    int b;
};
struct Derived : Base {
    int c;
    void setBase(const Base &from) { static_cast<Base &>(*this) = from; }
};
struct Node {
    virtual ~Node() {}
    virtual Index &index() { return mIndex; }
    Index mIndex;
};
static int take(Index byValue, int scale) { return byValue.value * scale; }
static int read(Index &ref) { return ref.value; }
int run() {
    Index i(5);
    int total = take(i, 10);
    total += take(Index(2), 100);
    Empty e;
    Box<Empty> box(e, 7);
    total += box.deleter().twice(box.payload);
    Base from;
    from.a = 3;
    from.b = 4;
    Derived d;
    d.c = 1000;
    d.setBase(from);
    total += d.a * d.b + d.c;
    Node node;
    node.mIndex.value = 9;
    Node *np = &node;
    total += read(np->index()) * 3;
    return total;
}
}
