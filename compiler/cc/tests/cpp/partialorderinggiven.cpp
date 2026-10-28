// Partial ordering weighs the parameters given arguments only: `dump(x,
// comma)` takes dump(list, comma), not dump(list, comparator, comma =
// ...) (WTF's sortedListDump).
#include <stdio.h>
struct Lit { const char* s; };
template<typename T, typename C> int dump(const T& list, const C& comparator, Lit comma = Lit { ", " }) { return 1 + comparator(list, list); }
template<typename T> int dump(const T& list, Lit comma = Lit { ", " }) { return 20 + dump(list, [](int a, int b) { return a < b; }, comma); }
int main() { int x = 3; printf("%d %d\n", dump(x, Lit { "; " }), dump(x)); return 0; }
