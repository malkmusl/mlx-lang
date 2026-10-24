// `V v(P(&node, offset));`: a variable initialized with a temporary
// made of an address and a value, not a function declaration
// (CompositeEditCommand's VisiblePosition(Position(&textNode, upstream))).
#include <stdio.h>
struct Text { int id; };
struct Position {
    Position(Text* node, int offset) : node(node), offset(offset) { }
    Text* node;
    int offset;
};
struct Visible {
    Visible(Position position) : position(position) { }
    Position position;
};
int main(int argc, char** argv)
{
    Text textNode { 7 };
    int upstream = 3;
    Visible visibleUpstream(Position(&textNode, upstream));
    Visible visibleDownstream(Visible(Position(&textNode, upstream + 1)));
    printf("%d %d %d\n", visibleUpstream.position.node->id, visibleUpstream.position.offset, visibleDownstream.position.offset);
    return argc - 1;
}
