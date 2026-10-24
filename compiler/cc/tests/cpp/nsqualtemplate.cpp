#include <cstdio>

namespace N {
struct Node { int v; };
bool allow(int, Node*);
namespace Security {
template<typename T> T* check(int, T&);
bool allowNode(int, Node*);
}
template<typename T> inline T* Security::check(int g, T& node)
{
    return allowNode(g, &node) ? &node : nullptr;
}
bool Security::allowNode(int g, Node* n) { return g > n->v; }
}

int main(int argc, char** argv)
{
    N::Node n { 1 };
    std::printf("%d %d\n", N::Security::check(argc, n) != nullptr, N::Security::check(0, n) != nullptr);
    return 0;
}
