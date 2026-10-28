// ranges::lower_bound with a projection and `{ }` for the comparison
// (WTF's SparseBitVector): std::ranges::less by default.
#include <stdio.h>
#include <algorithm>
#include <vector>
struct Element { unsigned index; int bits; };
int main()
{
    std::vector<Element> elements { { 1, 0 }, { 4, 0 }, { 9, 0 }, { 12, 0 } };
    auto at = std::ranges::lower_bound(elements, 5u, { }, &Element::index);
    auto first = std::ranges::lower_bound(elements, 12u, std::ranges::less { }, &Element::index);
    printf("%d %d\n", (int)(at - elements.begin()), (int)(first - elements.begin()));
    return 0;
}
