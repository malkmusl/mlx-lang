// A function returning an instance of a class template not made yet
// where the function is defined: the instance is made there, so its
// size decides that the result goes to the caller's buffer, and a local
// whose buffer points into itself (WTF::Vector's inline buffer) is moved
// into that buffer by its move constructor, not copied out by its bytes.
#include <cstdio>
template<typename T, int N> struct Storage { T* buffer; unsigned size; T inlineBuffer[N]; Storage() : buffer(inlineBuffer), size(0) {} };
template<typename T, int N> class List : private Storage<T, N> {
    using Base = Storage<T, N>;
public:
    List() {}
    List(List&&);
    void append(T x) { Base::buffer[Base::size++] = x; }
    bool isInline() const { return Base::buffer == Base::inlineBuffer; }
    unsigned size() const { return Base::size; }
};
template<typename T, int N> List<T, N>::List(List&& other) { Base::size = other.size(); for (unsigned i = 0; i < Base::size; i++) Base::inlineBuffer[i] = other.inlineBuffer[i]; }
using Pointers = List<void*, 1>;
static int x;
Pointers make() { Pointers result; result.append(&x); return result; }
int main() {
    Pointers list = make();
    printf("size %u inline %d\n", list.size(), list.isInline());
    return 0;
}
