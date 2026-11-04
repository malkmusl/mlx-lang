// A lambda calling a static member function that has member templates
// of its name (JavaScriptCore's selectScratchGPR): the arguments choose
// among all of them, as outside the lambda.
#include <stdio.h>
static int fsum() { return 0; }
template<typename... A> static int fsum(int x, A... rest) { return x + fsum(rest...); }
struct H {
    static int sum() { return 0; }
    template<typename... A> static int sum(int x, A... rest) { return x + sum(rest...); }
    static int two(int) { return 100; }
    static int two(int, int) { return 200; }
    int msum() { return 0; }
    template<typename... A> int msum(int x, A... rest) { return x + msum(rest...); }
};
struct J : H {
    int run() {
        int a = 1, b = 2, c = 4;
        auto h1 = [&] (int d) { return sum(a, b, c, d); };        // unqualified static overload set
        auto h2 = [&] (int d) { return H::sum(a, b, c, d); };     // qualified
        auto h3 = [&] (int d) { return fsum(a, b, c, d); };       // free overload set
        auto h4 = [&] (int d) { return two(a, d); };              // non-template overloads
        auto h5 = [&] (int d) { return msum(a, b, c, d); };       // non-static member overload set
        auto h6 = [this, a, b, c] (int d) { return sum(a, b, c, d); };
        auto h7 = [&] (int d) { return this->sum(a, b, c, d); };
        struct Local { static int go(H*, int a, int b, int c, int d) { return H::sum(a, b, c, d); } };
        return printf("h1=%d h2=%d h3=%d h4=%d h5=%d h6=%d h7=%d local=%d direct=%d\n", h1(8), h2(8), h3(8), h4(8), h5(8), h6(8), h7(8), Local::go(this, a, b, c, 8), sum(a, b, c, 8));
    }
};
int main() { J j; j.run(); return 0; }
