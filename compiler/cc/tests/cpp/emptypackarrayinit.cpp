// `Arg temp[] { arg, arguments... }` in a constructor template made with
// no more arguments (AirInst.h's Inst): the empty pack's expansion is no
// element (it was a 0, which an Arg has no constructor from).
#include <stdio.h>
struct Arg { Arg() : v(0) { } Arg(long x, int k) : v(x + k) { } long v; };
struct Inst {
    template<typename... Arguments>
    Inst(int kind, Arg arg, Arguments... arguments) : kind(kind)
    {
        Arg temp[] { arg, arguments... };
        n = sizeof(temp) / sizeof(Arg);
        total = 0;
        for (auto& a : temp)
            total += a.v;
    }
    int kind;
    long n;
    long total;
};
struct Pair { Arg a; Arg b; };
template<typename... A> Pair pair(Arg first, A... rest) { return Pair { first, rest... }; }
int main()
{
    Inst i(5, Arg(2, 0));
    Inst j(6, Arg(1, 0), Arg(2, 0), Arg(3, 0));
    Pair p = pair(Arg(4, 0));
    printf("%ld %ld %ld %ld %ld %ld\n", i.n, i.total, j.n, j.total, p.a.v, p.b.v);
    return 0;
}
