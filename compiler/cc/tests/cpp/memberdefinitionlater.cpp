// A class template's instance named first where it is not made (the
// result of a member function, or of a function only declared), made
// later in place, then a member template's definition read: applied to
// the instance once (twice, it took the other constructor template of
// as many parameters for the one it defines, and JSC's
// `AuxiliaryBarrier<Butterfly *>(butterfly, WriteBarrierEarlyInit)` had
// no constructor).
#include <cstdio>
#include <utility>

enum Tag { TagValue };
struct VM {
    int barriers = 0;
    void barrier(void *) { ++barriers; }
};
template <typename T> class Barrier {
public:
    Barrier() = default;
    template <typename U> Barrier(VM &, void *, U &&);
    template <typename U> Barrier(U &&value, Tag) : m_value(std::forward<U>(value)) {}
    template <typename U> void set(VM &, void *, U &&);
    const T &get() const { return m_value; }
private:
    T m_value {};
};
struct Butterfly { int x; };

class Object {
public:
    Barrier<Butterfly *> &ref() { return m_b; }
    Object(Butterfly *b);
private:
    Barrier<Butterfly *> m_b;
};
Barrier<int *> makeOne(int *);
Barrier<int *> global;

template <typename T> template <typename U> Barrier<T>::Barrier(VM &vm, void *owner, U &&value) : m_value(std::forward<U>(value)) { vm.barrier(owner); }
template <typename T> template <typename U> void Barrier<T>::set(VM &vm, void *owner, U &&value)
{
    m_value = std::forward<U>(value);
    vm.barrier(owner);
}

Object::Object(Butterfly *b) : m_b(b, TagValue) {}
Barrier<int *> makeOne(int *p) { return Barrier<int *>(p, TagValue); }

int main() {
    Butterfly b { 3 }, c { 4 };
    Object o(&b);
    VM vm;
    Barrier<Butterfly *> made(vm, nullptr, &c);
    std::printf("%d %d %d\n", o.ref().get()->x, made.get()->x, vm.barriers);
    o.ref().set(vm, nullptr, &c);
    int n = 7;
    global = makeOne(&n);
    std::printf("%d %d %d\n", o.ref().get()->x, *global.get(), vm.barriers);
    return 0;
}
