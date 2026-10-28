// A class's own member type before its base's of the name (WTF's
// Vector, whose Base is its VectorBuffer, derived from VectorBuffer<T, 0>
// whose Base is VectorBufferBase): `Base::adopt(...)` in the move
// constructor defined out of the class is the VectorBuffer's.
#include <stdio.h>
#include <utility>
namespace W {
template<typename T> class BufferBase {
public:
    void adoptBase(BufferBase&& o) { n = o.n; }
    int n = 0;
};
template<typename T, int cap> class Buffer;
template<typename T> class Buffer<T, 0> : private BufferBase<T> {
    typedef BufferBase<T> Base;
public:
    void adopt(Buffer&& o) { Base::adoptBase(std::move(o)); }
    using Base::n;
};
template<typename T, int cap = 0> class Vec : private Buffer<T, cap> {
    typedef Buffer<T, cap> Base;
public:
    Vec() { }
    Vec(Vec&& other);
    int size() const { return Base::n; }
    void set(int v) { Base::n = v; }
};
template<typename T, int cap>
inline Vec<T, cap>::Vec(Vec<T, cap>&& other)
{
    Base::adopt(std::forward<decltype(other)>(other));
}
template<typename T> class Trailing { public: static int alloc(int n) { return n * 2; } };
template<typename T> class Embedded : private Trailing<Embedded<T>> {
    using Base = Trailing<Embedded<T>>;
public:
    template<int cap>
    static int createFromVector(Vec<T, cap>&& other)
    {
        auto container = std::move(other);
        return Base::alloc(container.size());
    }
};
}
namespace J { namespace D {
struct Data { int x; };
int make() { W::Vec<Data> v; v.set(21); return W::Embedded<Data>::createFromVector(std::move(v)); }
} }
int main() { printf("%d\n", J::D::make()); return 0; }
