// Overloads given nested braces: the one whose parameter the list makes
// (a Vector from an initializer_list), not the first whose class has no
// such constructor (FontCascadeDescription::setFamilies({ { "system-ui"_s,
// FontFamilyKind::Generic } }) beside its Ref<RefCountedFixedVector>&&).
#include <stdio.h>
#include <vector>
#include <utility>
struct Family { const char* name; int kind; };
struct Fixed {
    explicit Fixed(unsigned size) : size(size) { }
    unsigned size;
};
struct Description {
    void setFamilies(const std::vector<Family>& families) { count = families.size(); first = families[0].name; }
    void setFamilies(Fixed& fixed) { count = fixed.size; }
    void setFamilies(Fixed&& fixed) { count = fixed.size + 100; }
    unsigned count = 0;
    const char* first = "";
};
int main(int argc, char** argv)
{
    Description description;
    description.setFamilies({ { "system-ui", 2 } });
    printf("%u %s\n", description.count, description.first);
    description.setFamilies(Fixed(3));
    printf("%u\n", description.count);
    return argc - 1;
}
