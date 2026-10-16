// WTF/JSC forms 21: conversions to built-in operands, hidden base members, offsetof through an alias
#include <stdio.h>
#include <stddef.h>

namespace p_convop {
struct Pos { int offset; Pos operator-(int a) const { return Pos { offset - a }; } operator int() const { return offset; } };
enum OpcodeID : unsigned { op_a = 3 };
struct CodeBlock {
    int store[8] = { 0, 10, 20, 30, 40, 50, 60, 70 };
    template<typename M> M &metadata(OpcodeID id, unsigned index) { return *reinterpret_cast<M *>(&store[id + index]); }
};
struct OpCall {
    static constexpr OpcodeID opcodeID = op_a;
    struct Metadata { int value; };
    Metadata &metadata(CodeBlock *codeBlock) const { return codeBlock->metadata<Metadata>(opcodeID, m_metadataID); }
    unsigned m_metadataID = 1;
};
int run() {
    Pos p { 0x12345 };
    int masked = (p - 5) & ~0xFFFF;
    CodeBlock cb; OpCall op;
    printf("%x %d\n", masked, op.metadata(&cb).value);
    return 0;
}
}

namespace p_hidemem {
template<class E> struct voidbase { E* error; voidbase() : error(nullptr) {} voidbase(E* e) : error(e) {} };
template<class T, class E> struct base { E* error; };
template<class T, class E> class ex : private base<T, E> {
    typedef base<T, E> b;
public:
    ex(E* e) { b::error = e; }
    E& error() { return *b::error; }
};
template<class E> class ex<void, E> : private voidbase<E> {
    typedef voidbase<E> b;
public:
    ex() {}
    ex(E* e) : b(e) {}
    explicit operator bool() const { return !b::error; }
    E& error() & { return *b::error; }
    const E& error() const & { return *b::error; }
};
struct Err { int k; };
static ex<void, Err> check(Err* e) { if (e) return ex<void, Err>(e); return { }; }
int run() { Err x{7}; auto r = check(&x); int v = 0; if (!r) v = r.error().k; ex<int, Err> q(&x); printf("%d %d\n", v, q.error().k); return 0; }
}

namespace p_hidefn {
enum OpcodeID : unsigned { op_a, op_b, op_c };
struct Base { OpcodeID opcodeID() const { return op_a; } int size() const { return 1; } };
struct CB { template<typename M> M& md(OpcodeID id, unsigned i) { static M m; m.v = id + i; return m; } };
struct Op : Base { struct M { int v; }; static constexpr OpcodeID opcodeID = op_c; static constexpr int size = 4; unsigned m_id = 5;
  M& md(CB* cb) const { return cb->md<M>(opcodeID, m_id); }
  int sz() const { return size + 1; } };
int run() { CB cb; Op op; printf("%d %d %d\n", op.md(&cb).v, op.sz(), Op::opcodeID); return 0; }
}

namespace p_hidefn2 {
struct Base { int size() const { return 1; } };
struct Op : Base { static constexpr int size = 4;
  int sz() const { return size + 1; } };
struct Op2 : Base { static const int size;
  int sz() const { return size + 1; } };
const int Op2::size = 6;
int run() { Op op; Op2 op2; printf("%d %d\n", op.sz(), op2.sz()); return 0; }
}

namespace p_offalias {
namespace J {
struct B { long a; };
template<unsigned n = 1> class Impl : public B {
public:
    friend class X;
    static constexpr unsigned count = n;
    static constexpr ptrdiff_t off() { return __builtin_offsetof(Impl, m_fields); }
protected:
    int m_fields[count] { };
};
class X {
    using Impl = J::Impl<>;
public:
    static const long* dummy();
};
const long* X::dummy() { static const long t[] = { 7, (long)(static_cast<unsigned>(__builtin_offsetof(Impl, m_fields))), (long)sizeof(Impl) }; return t; }
}
int run() { const long* t = J::X::dummy(); printf("%ld %ld %ld\n", t[0], t[1], t[2]); return 0; }
}

namespace p_opaqenum {
namespace J {
enum OpcodeID : unsigned;
enum OpcodeID : unsigned { op_a, op_b, op_c };
enum OpcodeID : unsigned;
struct CB { template<typename M> M& md(OpcodeID id, unsigned i) { static M m; m.v = id + i; return m; } };
enum OpcodeID : unsigned;
struct Op { struct M { static constexpr OpcodeID opcodeID = op_c; int v; }; static constexpr OpcodeID opcodeID = op_b; unsigned m_id = 5;
  M& md(CB* cb) const { return cb->md<M>(opcodeID, m_id); } };
}
int run() { J::CB cb; J::Op op; printf("%d\n", op.md(&cb).v); return 0; }
}

int main() {
    p_convop::run();
    p_hidemem::run();
    p_hidefn::run();
    p_hidefn2::run();
    p_offalias::run();
    p_opaqenum::run();
    return 0;
}
