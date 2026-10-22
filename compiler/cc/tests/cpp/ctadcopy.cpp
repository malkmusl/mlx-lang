// Class template argument deduction copying an instance with braces
// (`WeakHashSet registrations { m_registrations }`).
#include <stdio.h>
template<typename T> struct Set {
    Set() { }
    Set(const Set& other) : count(other.count + 1) { }
    int count = 0;
};
int main(int argc, char** argv)
{
    Set<int> original;
    Set copy { original };
    Set another(copy);
    printf("%d %d\n", copy.count, another.count);
    return argc - 1;
}
