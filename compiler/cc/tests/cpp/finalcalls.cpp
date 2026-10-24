// Calls that no class can override go straight to the function, as g++
// makes them: a `final` member function, and a member of a `final` class
// called on that class's object. WebKit's checked-pointer delete
// (WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR) calls one after running the
// object's destructor, when its vtable pointer is back at the base's,
// whose slot is pure.
#include <stdio.h>
#include <new>

struct Counted {
    virtual unsigned count() const = 0;
    virtual unsigned weight() const = 0;
protected:
    virtual ~Counted() = default;
};

struct Holder : Counted {
    unsigned n = 3;
    virtual unsigned weight() const { return 10; }
    unsigned count() const final { return n; }
};

struct Leaf final : Holder {
    unsigned weight() const override { return 20; }
    ~Leaf() { printf("~Leaf\n"); }
};

int main(int argc, char **argv)
{
    alignas(Leaf) unsigned char storage[sizeof(Leaf)];
    Leaf *leaf = new (storage) Leaf;
    Holder *holder = leaf;
    printf("count %u weight %u\n", holder->count(), holder->weight());
    printf("leaf count %u weight %u\n", leaf->count(), leaf->weight());
    // After the destructor the object is a Counted again: the calls
    // through Leaf still reach Leaf's and Holder's functions.
    leaf->Leaf::~Leaf();
    printf("after: count %u weight %u\n", leaf->count(), leaf->weight());
    return argc - 1;
}
