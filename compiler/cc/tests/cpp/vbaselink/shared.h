// Classes with virtual bases that code g++ compiled and code mlx-cc
// compiled both use: two hierarchies of the same shape, the G one's
// constructors and functions compiled by g++ (gpp.cpp), the M one's by
// mlx-cc (mlx.cpp). Each side makes objects of its own classes and of
// the other's (through the other's complete-object constructors, C1),
// and reads and writes the other's objects through their virtual bases
// (whose offsets are in the vtables, where the Itanium ABI puts them).
#define HIERARCHY(P) \
    struct P##Base { int base; P##Base(int b); int twice() const; }; \
    struct P##Left : virtual P##Base { int left; P##Left(int l); int sum() const; }; \
    struct P##Right : virtual P##Base { long right; P##Right(long r); }; \
    struct P##Both : P##Left, P##Right { short both; P##Both(int b, int l, long r, short s); }; \
    struct P##Extra { char extra[3]; }; \
    struct P##Far : P##Extra, virtual P##Both { int far; P##Far(int f); }; \
    struct P##Nearly { virtual int nearly() const; virtual void other(); }; \
    struct P##Prim : virtual P##Nearly, virtual P##Base { int prim; P##Prim(int p); int nearly() const override; };

// Virtual functions over virtual bases (the vcall offsets before the
// vbase offsets), a virtual base of its own beside the diamond's.
#define FUNCTIONS(P) \
    struct P##V { int v; P##V(); virtual int f() const; virtual int g() const; virtual ~P##V(); }; \
    struct P##L : virtual P##V { int l; P##L(); int f() const override; virtual int h() const; }; \
    struct P##R : virtual P##V { int r; P##R(); int g() const override; virtual int k() const; }; \
    struct P##J : P##L, P##R { int j; P##J(); int h() const override; }; \
    struct P##X { long x; P##X(); virtual long ex() const; virtual ~P##X(); }; \
    struct P##K : virtual P##X, P##J { int kk; P##K(); int f() const override; long ex() const override; };

HIERARCHY(G)
HIERARCHY(M)
FUNCTIONS(G)
FUNCTIONS(M)

// Made by g++'s code: its own objects and mlx-cc's.
GBoth *gMakeBoth(int k);
GFar *gMakeFar(int k);
MBoth *gMakeMBoth(int k);
// Read and written by g++'s code through the virtual bases.
long gReadLeft(const MLeft *l);
long gReadRight(const MRight &r);
void gSetBase(MBoth *b, int value);
long gReadFar(const MFar *f);
const MBase *gBaseOf(const MRight *r);
GPrim *gMakePrim(int k);
long gReadPrim(const MPrim *p);
GK *gMakeK();
long gReadK(const MK *k);
long gReadR(const MR *r);
