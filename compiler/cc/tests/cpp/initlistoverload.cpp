// A braced list of one pointer chooses the constructor whose
// initializer_list takes that pointer, not another pointer type's
// (ANGLE's TIntermDeclaration({declarator}) of SeparateDeclarations).
#include <initializer_list>
#include <stdio.h>

struct Variable { int id; };
struct Node { virtual ~Node() {} int line = 3; };
struct Typed : Node { int type = 7; };

struct Declaration {
    Declaration() {}
    Declaration(const Variable *variable, Typed *init) { printf("variable and init\n"); }
    Declaration(std::initializer_list<const Variable *> variables) { printf("variables %zu\n", variables.size()); }
    Declaration(std::initializer_list<Typed *> declarators) { printf("declarators %zu %d\n", declarators.size(), (*declarators.begin())->type); }
    Declaration(const Declaration &) { printf("copy\n"); }
};

int main() {
    Typed typed;
    Typed *declarator = &typed;
    Variable variable { 1 };
    const Variable *pointer = &variable;
    Declaration *a = new Declaration({declarator});
    Declaration *b = new Declaration({pointer});
    Declaration c({declarator, declarator});
    delete a;
    delete b;
    return 0;
}
