// An operator-> giving an object of a class is applied again until a
// pointer (WebCore's SQLiteStatementAutoResetScope::operator-> gives a
// CheckedPtr<SQLiteStatement>: "invalid pointer dereference").
#include <stdio.h>

struct Statement { int bound = 0; int bind(int v) { bound += v; return bound; } };
template<typename T> struct Checked {
    T *p;
    T *operator->() const { return p; }
};
struct Scope {
    Statement *s;
    Checked<Statement> operator->() { return Checked<Statement> { s }; }
};
struct Outer {
    Scope scope;
    Scope operator->() { return scope; }
};

int main(int argc, char **argv)
{
    Statement s;
    Scope scope { &s };
    Outer outer { scope };
    printf("%d %d\n", scope->bind(3), outer->bind(4));
    return argc;
}
