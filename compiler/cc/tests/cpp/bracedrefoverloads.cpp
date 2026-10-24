// Overloads given nested braces: a Ref<T> made of { { name, kind } } is
// none, neither through Ref(T &), which binds no list's temporary, nor
// through a constructor template taking a Ref<U> &&, which a list
// deduces nothing for (FontCascadeDescription::setFamilies({ {
// "system-ui"_s, FontFamilyKind::Generic } }) beside its
// Ref<RefCountedFixedVector<FontFamily>> && overload).
#include <stdio.h>
#include <vector>
#include <utility>
#include <initializer_list>
struct Family { const char* name; int kind; };
struct Fixed {
    Fixed(std::initializer_list<Family> list) : size(list.size()) { }
    explicit Fixed(unsigned size) : size(size) { }
    unsigned size;
};
template<typename T> struct Ref {
    Ref(T& t) : p(&t) { }
    Ref(const Ref& o) : p(o.p) { }
    Ref(Ref&& o) : p(o.p) { }
    template<typename U> Ref(Ref<U>&& o) : p(o.p) { }
    T* p;
};
struct Description {
    void setFamilies(const std::vector<Family>& families) { count = families.size(); first = families[0].name; }
    void setFamilies(Fixed& fixed) { count = fixed.size; }
    void setFamilies(Ref<Fixed>&& fixed) { count = fixed.p->size + 100; }
    unsigned count = 0;
    const char* first = "";
};
int main(int argc, char** argv)
{
    Description description;
    description.setFamilies({ { "system-ui", 2 } });
    printf("%u %s\n", description.count, description.first);
    Fixed f(3);
    description.setFamilies(Ref<Fixed>(f));
    printf("%u\n", description.count);
    return argc - 1;
}
