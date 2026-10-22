// Argument-dependent lookup through a pointer to a class declared only
// (`struct OpaqueContext;`, JavaScriptCore's OpaqueJSContext of a
// JSContextRef): the class's namespace, the global one, has the function
// the namespace around the call hides with functions of the name that
// do not take the argument (WebCore's toJS(JSContextRef) in
// SerializedScriptValue.cpp, "too few arguments").
#include <cstdio>
struct OpaqueContext;
typedef const OpaqueContext* ContextRef;
struct Global { int v; };
inline int toJS(ContextRef) { return 1; }
inline int toJS(Global* g) { return g->v; }
namespace W {
struct Obj { int v; };
inline int toJS(Obj*, Obj*, int extra) { return 100 + extra; }
template<typename T> int toJS(Obj&, T&&) { return 200; }
inline int viaContext(ContextRef c) { return toJS(c); }
inline int viaGlobal(Global* g) { return toJS(g); }
inline int own(Obj* o) { return toJS(o, o, 5); }
}
int main()
{
    Global g { 7 };
    W::Obj o { 3 };
    printf("%d %d %d\n", W::viaContext(nullptr), W::viaGlobal(&g), W::own(&o));
    return 5;
}
