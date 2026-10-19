// Members of a class template's instance specialized explicitly through
// a typedef of the instance before the instance is made: the in-class
// body of a specialized member is not made too (WebCore's bindings:
// `template<> void JSHTMLModElementDOMConstructor::initializeProperties(...)`).
#include <stdio.h>
struct Info { const char *n; };
template<class C> struct NC { static const Info s_info; static int proto(); void init() { printf("generic\n"); } int run() { init(); return proto(); } };
using X = NC<int>;
template<> const Info X::s_info = { "x" };
template<> int X::proto() { return 2; }
template<> void X::init() { printf("special\n"); }
int main(int argc, char **argv) { X x; printf("%d %s\n", x.run() + argc, X::s_info.n); return argc - 2; }
