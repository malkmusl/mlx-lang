// A base instance's template parameter is no member of the derived class:
// `Graph` in BackDominators is D::Graph, not Dominators<CFG>'s parameter
// (WTF's Dominators<Graph> and the DFG's BackwardsDominators).
#include <stdio.h>
#include <memory>
namespace W {
template<typename Graph> class Dominators {
public:
    Dominators(Graph& graph, bool selfCheck = false) : m_size(graph.size()) { }
    int m_size;
};
}
namespace D {
class Graph;
struct CFG { Graph& graph; int size() { return 7; } };
class Graph { public: int n { 3 }; CFG& cfg() { static CFG c { *this }; return c; } };
class BackDominators : public W::Dominators<CFG> {
public:
    BackDominators(Graph& graph) : W::Dominators<CFG>(graph.cfg()), m_n(graph.n) { }
    int m_n;
};
}
int main() { D::Graph g; auto d = std::make_unique<D::BackDominators>(g); printf("%d %d\n", d->m_size, d->m_n); return 0; }
