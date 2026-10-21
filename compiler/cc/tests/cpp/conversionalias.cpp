// A conversion operator to a class's own alias defined outside the class
// (`ImageDataArray::operator DataVariant() const`).
#include <stdio.h>
struct Array {
    using Variant = long;
    operator Variant() const;
    int value = 7;
};
Array::operator Variant() const
{
    return value * 2;
}
int main(int argc, char** argv)
{
    Array array;
    long v = array;
    printf("%ld\n", v);
    return argc - 1;
}
