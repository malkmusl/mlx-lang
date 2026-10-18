// Statics of inline functions and of a template's functions, and an
// inline variable: one object for the program however many files use
// them (each file's copy of the function finds the same one).
#include <stdio.h>
struct Counter { int n; Counter() : n(0) { printf("made\n"); } };
inline Counter &single() { static Counter c; return c; }
inline int &plain() { static int x = 5; return x; }
template <class T> T &tsingle() { static T t; return t; }
inline int sharedCount = 0;
inline int &twice() {
    { static int a = 1; a++; }
    static int a = 100;
    return a;
}
