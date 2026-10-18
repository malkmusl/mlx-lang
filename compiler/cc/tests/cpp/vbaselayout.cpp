// Virtual bases laid out as the Itanium C++ ABI says (g++ on x86-64):
// the sizes, alignments and offsets of the subobjects, found by
// pointer differences, for diamonds, nearly empty virtual bases chosen
// as primary bases, empty virtual bases, tail padding reused, and
// virtual bases of virtual bases.
#include <stdio.h>

template <class B, class D> long at(D &d) { return (char *)static_cast<B *>(&d) - (char *)&d; }

// The diamond.
struct V { int v = 1; };
struct B1 : virtual V { int b1 = 2; };
struct B2 : virtual V { int b2 = 3; };
struct D : B1, B2 { int d = 4; };

// A nearly empty virtual base (only a vtable pointer): the primary base.
struct N { virtual int n() { return 10; } };
struct P1 : virtual N { long p1 = 11; };
struct P2 : virtual N { int p2 = 12; };
struct PD : P1, P2 { char pd = 13; };

// An empty virtual base, a virtual base with a double.
struct E {};
struct WithE : virtual E { int x = 5; };
struct Dbl { double d = 1.5; };
struct WithDbl : virtual Dbl { char c = 6; };
struct Both : WithE, WithDbl { short s = 7; };

// Tail padding of a base with virtual bases reused by the derived class.
struct T : virtual V { int a = 8; char c = 9; };
struct TD : T { char e = 10; };

// Polymorphic bases and virtual bases together, virtual bases of
// virtual bases.
struct A { virtual ~A() {} int a = 20; };
struct C : virtual A { int c = 21; };
struct G : virtual C { int g = 22; };
struct H : virtual C, virtual A { int h = 23; };
struct K : G, H { int k = 24; };

// A class with a non-virtual base and a virtual one of the same kind.
struct M1 : A, virtual V { int m = 30; };
struct M2 : virtual M1, B1 { int n = 31; };

int main(int argc, char **argv) {
    D d;
    printf("V %zu %zu B1 %zu %zu B2 %zu %zu D %zu %zu\n", sizeof(V), alignof(V), sizeof(B1), alignof(B1), sizeof(B2), alignof(B2), sizeof(D), alignof(D));
    printf("D: B1 %ld B2 %ld V %ld V-via-B2 %ld\n", at<B1>(d), at<B2>(d), at<V>(d), (char *)static_cast<V *>(static_cast<B2 *>(&d)) - (char *)&d);
    printf("D: v %ld b1 %ld b2 %ld d %ld\n", (char *)&d.v - (char *)&d, (char *)&d.b1 - (char *)&d, (char *)&d.b2 - (char *)&d, (char *)&d.d - (char *)&d);
    printf("D values %d %d %d %d\n", d.v, d.b1, d.b2, d.d);

    PD pd;
    printf("N %zu P1 %zu P2 %zu PD %zu %zu\n", sizeof(N), sizeof(P1), sizeof(P2), sizeof(PD), alignof(PD));
    printf("PD: P1 %ld P2 %ld N %ld p2 %ld pd %ld\n", at<P1>(pd), at<P2>(pd), at<N>(pd), (char *)&pd.p2 - (char *)&pd, (char *)&pd.pd - (char *)&pd);
    printf("PD values %d %ld %d %d\n", pd.n(), pd.p1, pd.p2, pd.pd);

    Both both;
    printf("E %zu WithE %zu WithDbl %zu %zu Both %zu %zu\n", sizeof(E), sizeof(WithE), sizeof(WithDbl), alignof(WithDbl), sizeof(Both), alignof(Both));
    printf("Both: WithE %ld WithDbl %ld E %ld Dbl %ld s %ld\n", at<WithE>(both), at<WithDbl>(both), at<E>(both), at<Dbl>(both), (char *)&both.s - (char *)&both);
    printf("Both values %d %g %d %d\n", both.x, both.d, both.c, both.s);

    TD td;
    printf("T %zu TD %zu: e %ld V %ld\n", sizeof(T), sizeof(TD), (char *)&td.e - (char *)&td, at<V>(td));
    printf("TD values %d %d %d %d\n", td.v, td.a, td.c, td.e);

    K k;
    printf("A %zu C %zu G %zu H %zu K %zu\n", sizeof(A), sizeof(C), sizeof(G), sizeof(H), sizeof(K));
    printf("K: G %ld H %ld C %ld A %ld\n", at<G>(k), at<H>(k), at<C>(k), at<A>(k));
    printf("K values %d %d %d %d %d\n", k.a, k.c, k.g, k.h, k.k);
    G g;
    printf("G: C %ld A %ld g %d c %d a %d\n", at<C>(g), at<A>(g), g.g, g.c, g.a);

    M2 m2;
    printf("M1 %zu M2 %zu\n", sizeof(M1), sizeof(M2));
    printf("M2: M1 %ld B1 %ld V %ld A %ld\n", at<M1>(m2), at<B1>(m2), at<V>(m2), at<A>(m2));
    printf("M2 values %d %d %d %d %d\n", m2.a, m2.m, m2.n, m2.b1, m2.v);
    return argc + 1;
}
