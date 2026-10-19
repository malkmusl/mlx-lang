// `&Derived::m` of a data member declared in a base is a pointer to a
// member of that base, as C++ types it, made one of the derived class's
// (its offset moved) where one is wanted (WebCore's CSSPrimitiveValue
// static_asserts `is_member_object_pointer_v<decltype(&CSSPrimitiveValue::
// m_hasCachedCSSText)>`, a member of CSSValue). Read as `&this->m` in a
// member function it was a plain pointer.
#include <stdio.h>
#include <type_traits>

struct A { int a { 1 }; };
struct B { int b { 2 }; int c { 3 }; };
struct D : A, B { int d { 4 }; };

class Value {
protected:
    int kind { 1 };
    mutable unsigned char m_cached { 7 };
};

class Primitive : public Value {
public:
    int serialize() const
    {
        static_assert(std::is_member_object_pointer_v<decltype(&Primitive::m_cached)>);
        static_assert(std::is_same_v<decltype(&Primitive::m_cached), unsigned char Value::*>);
        auto p = &Primitive::m_cached;
        return this->*p + kind;
    }
};

int main(int argc, char **)
{
    Primitive p;
    D d;
    int B::*pc = &D::c;
    int D::*pd = &D::d;
    int D::*pb = &D::b;
    static_assert(std::is_same_v<decltype(&D::b), int B::*>);
    printf("%d %d %d %d %d\n", p.serialize(), d.*pc, d.*pd, d.*pb, (int)(pb == &D::b));
    return argc;
}
