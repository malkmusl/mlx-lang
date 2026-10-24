// An explicit instantiation (or specialization) names the function
// template of its own namespace, not one a using-declaration brings in
// from outside: `template bool contains<Tree>(const SimpleRange &, const
// BoundaryPoint &);` in WebCore beside a global `using WTF::contains;`.
#include <cstdio>
#include <optional>
#include <span>
namespace WTF {
template<typename T, std::size_t TExtent, typename U, std::size_t UExtent>
requires(sizeof(T) == 1)
bool contains(std::span<T, TExtent> haystack, std::span<U, UExtent> needle) { return false; }
}
using WTF::contains;
namespace WebCore {
enum TreeType { Tree, ShadowIncludingTree, ComposedTree };
struct Node { int v; };
struct BoundaryPoint { int offset; };
struct SimpleRange { BoundaryPoint start, end; };
template<TreeType = Tree> bool contains(const SimpleRange&, const BoundaryPoint&);
template<TreeType = Tree> bool contains(const SimpleRange&, const std::optional<BoundaryPoint>&);
template<TreeType = Tree> bool contains(const SimpleRange& outerRange, const SimpleRange& innerRange);
template<TreeType = Tree> bool contains(const SimpleRange&, const Node&);
template<> bool contains<ComposedTree>(const SimpleRange&, const std::optional<BoundaryPoint>&);
bool contains(TreeType, const SimpleRange& outerRange, const SimpleRange& innerRange);
bool contains(TreeType, const SimpleRange&, const BoundaryPoint&);

template<TreeType treeType> bool contains(const SimpleRange& range, const BoundaryPoint& point)
{
    return range.start.offset <= point.offset && point.offset <= range.end.offset + treeType;
}
template bool contains<Tree>(const SimpleRange&, const BoundaryPoint&);
template<TreeType treeType> bool contains(const SimpleRange& range, const std::optional<BoundaryPoint>& point)
{
    return point && contains<treeType>(range, *point);
}
template<> bool contains<ComposedTree>(const SimpleRange& range, const std::optional<BoundaryPoint>& point)
{
    return point && contains<ComposedTree>(range, *point);
}
bool contains(TreeType type, const SimpleRange& range, const BoundaryPoint& point)
{
    switch (type) {
    case Tree: return contains<Tree>(range, point);
    case ShadowIncludingTree: return contains<ShadowIncludingTree>(range, point);
    case ComposedTree: return contains<ComposedTree>(range, point);
    }
    return false;
}
template<TreeType treeType> bool contains(const SimpleRange& outerRange, const SimpleRange& innerRange)
{
    return contains<treeType>(outerRange, innerRange.start) && contains<treeType>(outerRange, innerRange.end);
}
template bool contains<Tree>(const SimpleRange&, const SimpleRange&);
template bool contains<ComposedTree>(const SimpleRange&, const SimpleRange&);
}
using namespace WebCore;
int main() {
    SimpleRange r { { 1 }, { 5 } };
    printf("%d %d %d\n", contains(r, BoundaryPoint { 3 }), contains(ComposedTree, r, BoundaryPoint { 6 }), contains(r, SimpleRange { { 2 }, { 4 } }));
}
