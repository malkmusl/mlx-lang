// A class ending with an array of no elements of a class without a
// default constructor (GNU's `Attribute m_attributeArray[0]` of WebCore's
// ShareableElementData): its constructor makes no element of it, its
// destructor destroys none.
#include <cstdio>
#include <cstdlib>
#include <new>
static int destroyed = 0;
struct Attribute {
    Attribute(int v) : value(v) { }
    Attribute(const Attribute& other) : value(other.value + 100) { }
    ~Attribute() { destroyed++; }
    int value;
};
struct Base { unsigned size; Base(unsigned s) : size(s) { } };
struct Shareable : Base {
    Shareable(const int* values, unsigned n) : Base(n)
    {
        for (unsigned i = 0; i < n; ++i)
            new (&m_array[i]) Attribute(values[i]);
    }
    Shareable(const Shareable& other) : Base(other.size)
    {
        for (unsigned i = 0; i < size; ++i)
            new (&m_array[i]) Attribute(other.m_array[i]);
    }
    ~Shareable()
    {
        for (unsigned i = 0; i < size; ++i)
            m_array[i].~Attribute();
    }
    Attribute m_array[0];
};
int main() {
    int values[3] = { 4, 5, 6 };
    void* p = malloc(sizeof(Shareable) + 3 * sizeof(Attribute));
    Shareable* s = new (p) Shareable(values, 3);
    printf("%u %d %d\n", s->size, s->m_array[0].value, s->m_array[2].value);
    void* q = malloc(sizeof(Shareable) + 3 * sizeof(Attribute));
    Shareable* t = new (q) Shareable(*s);
    printf("%u %d %d\n", t->size, t->m_array[0].value, t->m_array[2].value);
    s->~Shareable();
    t->~Shareable();
    printf("%d %d\n", destroyed, (int)sizeof(Shareable));
    free(p);
    free(q);
}
