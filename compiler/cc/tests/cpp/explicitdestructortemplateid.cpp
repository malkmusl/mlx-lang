// `w.~W<T, Tag<T, int>>()`: an explicit destructor call naming the class
// template's instance with its arguments (WTF's ThreadSafeWeakOrStrongPtr).
#include <stdio.h>
template<typename T, typename U> struct W { ~W() { printf("~W\n"); } int x; };
template<typename T, typename S> struct Tag { };
template<typename T> struct H {
    union {
        W<T, Tag<T, int>> w;
        int i;
    };
    H() : w() { }
    ~H() { w.~W<T, Tag<T, int>>(); }
};
int main() { H<int> h; return 0; }
