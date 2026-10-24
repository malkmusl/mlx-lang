// A friend function declared in a nested class is of the namespace
// around the classes, not of the outer class: WebCore's XPath::Step::
// NodeTest befriends nodeMatches, which XPath defines and Step's members
// call (they named Step::nodeMatches, defined nowhere).
#include <cstdio>
namespace N {
class Step {
public:
    enum Axis { A, B };
    class NodeTest {
    public:
        int kind = 3;
    private:
        friend class Step;
        friend bool nodeMatches(int, Axis, const NodeTest&);
    };
    int run(int n) const;
    NodeTest test;
};
inline bool nodeMatches(int n, Step::Axis, const Step::NodeTest& t)
{
    return n == t.kind;
}
int Step::run(int n) const
{
    return nodeMatches(n, A, test) ? 1 : 0;
}
}
int main()
{
    N::Step s;
    std::printf("%d %d\n", s.run(3), s.run(4));
    return 0;
}
