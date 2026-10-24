// Forms with virtual bases: value-initialization and braces, arrays made
// by new[], templates over and as virtual bases, a class whose only base
// is virtual, a virtual base reached through a non-virtual one, casts up
// and down along non-virtual paths, pointer comparisons across paths,
// objects passed by value.
#include <stdio.h>
#include <new>

struct V {
    int v;
    V() : v(3) {}
    V(int x) : v(x) {}
    virtual ~V() { printf("~V %d\n", v); }
    virtual int get() const { return v; }
};

struct Only : virtual V {};

struct Holder : virtual V {
    int h = 9;
    Holder() {}
    explicit Holder(int x) : V(x), h(x + 1) {}
};

template <class T> struct Box : virtual V {
    T value{};
    Box() : V(40) {}
    int get() const override { return v + (int)value; }
};

template <class Base> struct Wrap : virtual Base {
    int w = 5;
};

struct Plain { int p = 1; };
struct Mid : Plain, Holder { int m = 2; };
struct Leaf : Mid, Box<char> {
    Leaf() : V(70) {}
};

int byValue(Holder h) { return h.v * 100 + h.h; }

int main(int argc, char **argv) {
    Only o{};
    printf("only %d %zu\n", o.v, sizeof(Only));
    Holder h1{};
    Holder h2(11);
    Holder *h3 = new Holder();
    printf("holders %d %d %d %d %d\n", h1.v, h1.h, h2.v, h2.h, h3->v);
    delete h3;

    Box<short> box;
    box.value = 2;
    const V &asV = box;
    printf("box %d %d\n", box.get(), asV.get());

    Wrap<Holder> wrapped;
    printf("wrap %d %d %d sizes %zu %zu\n", wrapped.w, wrapped.h, wrapped.v, sizeof(Wrap<Holder>), sizeof(Box<short>));

    Leaf leaf;
    Mid *mid = &leaf;
    V *vp = mid;
    Holder *hp = &leaf;
    Box<char> *bp = &leaf;
    printf("leaf %d %d %d %d same %d %d\n", leaf.get(), vp->get(), hp->v, bp->v, vp == static_cast<V *>(bp), (V *)hp == vp);
    Leaf *back = static_cast<Leaf *>(mid);
    Mid *mid2 = static_cast<Mid *>(hp);
    printf("back %d %d %d\n", back == &leaf, mid2 == mid, static_cast<Plain *>(back)->p);

    Holder *array = new Holder[3];
    for (int i = 0; i < 3; i++) array[i].v = i + 20;
    int passed = byValue(array[2]);
    printf("array %d %d\n", array[1].get(), passed);
    delete[] array;

    alignas(Holder) unsigned char buffer[sizeof(Holder)];
    Holder *placed = new (buffer) Holder(30);
    printf("placed %d %d\n", placed->get(), placed->h);
    placed->~Holder();
    printf("end\n");
    return argc + 1;
}
