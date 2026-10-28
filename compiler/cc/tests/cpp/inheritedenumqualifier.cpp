// A member's qualified name through a base's type before a namespace's of
// that name: `AccessType::ModuleNamespaceLoad` in a class derived from
// JSC's AccessCase, whose enum AccessType hides the enum class
// JSC::AccessType (ModuleNamespaceAccessCase.cpp).
#include <stdio.h>
namespace J {
enum class AccessType : signed char { GetById, PutById };
class Base {
public:
    enum AccessType : unsigned char { Load, Transition, ModuleLoad };
    Base(int x, AccessType t) : m_x(x), m_type(t) { }
    int m_x;
    AccessType m_type;
};
class Derived : public Base {
public:
    Derived(int x) : Base(x, AccessType::ModuleLoad) { }
    int kind() const { return AccessType::Transition; }
};
}
int main() { J::Derived d(3); printf("%d %d %d\n", d.m_x, d.m_type, d.kind()); return 0; }
