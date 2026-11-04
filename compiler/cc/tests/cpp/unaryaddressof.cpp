// `&x` of a class with a unary operator& is that operator
// ([over.match.oper]); std::addressof and __builtin_addressof give the
// object's own address. WTF's UniqueRef<T>::operator& gives its T's
// address: `&it->value` of ViewTransition's HashMap<AtomString,
// UniqueRef<CapturedElement>> was the UniqueRef's own address read as a
// CapturedElement (Voyager crashed on YouTube in captureNewState).
// mlxlibcpp's containers keep their elements' own addresses.
#include <stdio.h>
#include <memory>
#include <optional>
#include <vector>

struct Element { int a; int b; };
template<typename T> struct Box {
    T* p;
    T* operator&() const { return p; }
};
struct Pair { int key; Box<Element> value; };
static Element* find(Pair* it) { return &it->value; }

struct Heap;
struct Reference { int* slot; Heap* operator&() const { return (Heap*)slot; } };

struct Bits {
    int v;
    int operator&(int mask) const { return v & mask; }
};
struct Free { int v; };
static int freeCalls = 0;
static Free* operator&(Free& f) { freeCalls++; return std::addressof(f); }

int main()
{
    Element e { 7, 9 };
    Pair pair { 1, { &e } };
    Box<Element> box { &e };
    printf("member operator&: %d %d, b %d\n", find(&pair) == &e, &box == &e, find(&pair)->b);
    Box<Element>* own = std::addressof(box);
    printf("addressof: %d %d\n", (void*)own != (void*)&e, __builtin_addressof(box) == own);
    std::optional<Box<Element>> held = box;
    std::vector<Box<Element>> list { box, box, box };
    list.push_back(box);
    printf("containers: %d %d %zu\n", held->p == &e, list[3].p == &e, list.size());
    int target = 5;
    Reference r { &target };
    printf("reference: %d\n", (void*)&r == (void*)&target);
    Bits bits { 6 };
    printf("binary operator& %d, own address %d\n", bits & 3, (&bits)->v);
    Free f { 4 };
    Free* fp = &f;
    printf("free operator& %d %d\n", fp->v, freeCalls);
    return 0;
}
