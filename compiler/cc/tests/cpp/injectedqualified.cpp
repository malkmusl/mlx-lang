// A class template's name inside its instance, qualifying a member type
// (`STATIC_ASSERT_ISO_SUBSPACE_SHARABLE(JSDOMIteratorPrototype,
// JSDOMIteratorPrototype::Base)` in WebCore's JSDOMIterator.h, a
// `sizeof(Proto::Base)` there: "undeclared identifier 'Base'"): the
// instance's, in sizeof, a declaration and a static member's call.
#include <stdio.h>

struct Object {
    int x;
    static int destroy() { return 7; }
};
template<typename W, typename T> class Proto final : public Object {
public:
    using Base = Object;
    static int create()
    {
        static_assert(sizeof(Proto) == sizeof(Proto::Base));
        Proto::Base o { 3 };
        return o.x + Proto::Base::destroy() + (int)sizeof(Proto::Base);
    }
};

int main(int argc, char **argv)
{
    printf("%d\n", Proto<int, char>::create());
    return argc;
}
