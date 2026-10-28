// A generic lambda in a lambda of a member function naming a member: the
// outer lambda captures `this` for it (its body is read for the
// instance only, the outer's done): the DFG's ValueRepReductionPhase.
#include <stdio.h>
struct Graph { int n; int dump(int x) { return n + x; } };
struct File {
    template<typename F> int atomically(const F& f) { return f(*this); }
};
File& dataFile() { static File file; return file; }
struct Phase {
    Phase(Graph& g) : m_graph(g) { }
protected:
    Graph& m_graph;
};
struct Reduction : public Phase {
    Reduction(Graph& g) : Phase(g) { }
    int run()
    {
        int total = 0;
        for (int candidate = 1; candidate <= 2; ++candidate) {
            auto dumpEscape = [&](int node) {
                return dataFile().atomically([&](auto&) {
                    return m_graph.dump(candidate) * 10 + m_graph.dump(node);
                });
            };
            total += dumpEscape(candidate * 3);
        }
        return total;
    }
};
int main() { Graph g { 100 }; Reduction r(g); printf("%d\n", r.run()); return 0; }
