// std::remove_if and std::unique over unique_ptrs (Skia's SkSL module
// loader drops a module's function prototypes so): the kept elements are
// moved, not copied, and the vector's erase and shrink_to_fit leave them
// owned once (mlxlibcpp's remove_if copied them, which the deleted copy
// assignment should have refused: the erased tail freed the kept ones).
#include <algorithm>
#include <cstdio>
#include <memory>
#include <vector>

struct Element {
    int kind;
    explicit Element(int k) : kind(k) { }
    virtual ~Element() { std::printf("~%d ", kind); }
};

int main()
{
    std::vector<std::unique_ptr<Element>> elements;
    for (int i = 0; i < 9; ++i)
        elements.push_back(std::make_unique<Element>(i));
    elements.erase(std::remove_if(elements.begin(), elements.end(), [](const std::unique_ptr<Element>& e) { return e->kind % 3 == 2; }), elements.end());
    std::printf("\nkept %zu:", elements.size());
    for (auto& e : elements)
        std::printf(" %d", e->kind);
    elements.shrink_to_fit();
    std::vector<std::unique_ptr<int>> numbers;
    for (int n : { 1, 1, 2, 2, 2, 3 })
        numbers.push_back(std::make_unique<int>(n));
    auto end = std::unique(numbers.begin(), numbers.end(), [](const std::unique_ptr<int>& a, const std::unique_ptr<int>& b) { return *a == *b; });
    numbers.erase(end, numbers.end());
    std::printf("\nunique %zu:", numbers.size());
    for (auto& n : numbers)
        std::printf(" %d", *n);
    std::printf("\n");
    elements.clear();
    std::printf("\n");
    return 0;
}
