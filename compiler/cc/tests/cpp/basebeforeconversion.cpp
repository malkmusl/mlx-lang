// A derived class bound to a reference to its base is a standard
// conversion, better than any user-defined one, `T &&` or by value
// (WTF's WeakHashSet::add(const T &) for a PseudoElement, not
// add(WeakRef<T> &&) through WeakRef's constructor from a T).
#include <stdio.h>

struct Element { int id = 1; };
struct Pseudo : Element { };

template<class T> struct WeakRef {
    WeakRef(const T &object) : id(object.id + 10) { }
    int id;
};

struct Set {
    int add(const Element *) { return 1; }
    int add(const Element &value) { return 100 + add(&value); }
    int add(WeakRef<Element> &&value) { return 300 + value.id; }
};

struct Holder {
    Holder(const Element &e) : id(e.id + 20) { }
    int id;
};

int take(const Element &) { return 1; }
int take(Holder h) { return 2 + h.id; }

struct Text {
    Text(const char *) { }
};
int named(const Text &) { return 1; }
int named(Text &&) { return 2; }

int main(int argc, char **)
{
    Set set;
    Pseudo p;
    Element &e = p;
    printf("%d %d %d\n", set.add(p), set.add(e), set.add(WeakRef<Element>(p)));
    printf("%d %d\n", take(Pseudo()), take(p));
    printf("%d\n", named("x"));
    return argc;
}
