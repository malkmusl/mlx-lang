// The mlx-cc side of vbaselink (see shared.h).
#include <stdio.h>
#include "shared.h"

MBase::MBase(int b) : base(b) {}
int MBase::twice() const { return base * 2; }
MLeft::MLeft(int l) : MBase(-1), left(l) {}
int MLeft::sum() const { return base + left; }
MRight::MRight(long r) : MBase(-2), right(r) {}
MBoth::MBoth(int b, int l, long r, short s) : MBase(b), MLeft(l), MRight(r), both(s) {}
int MNearly::nearly() const { return 1; }
void MNearly::other() {}
MPrim::MPrim(int p) : MBase(p * 3), prim(p) {}
int MPrim::nearly() const { return prim; }
MV::MV() : v(1) {}
int MV::f() const { return v; }
int MV::g() const { return v; }
MV::~MV() {}
ML::ML() : l(2) {}
int ML::f() const { return l; }
int ML::h() const { return l; }
MR::MR() : r(3) {}
int MR::g() const { return r; }
int MR::k() const { return r; }
MJ::MJ() : j(4) {}
int MJ::h() const { return j; }
MX::MX() : x(5) {}
long MX::ex() const { return x; }
MX::~MX() {}
MK::MK() : kk(6) { v = 10; l = 20; r = 30; j = 40; x = 50; }
int MK::f() const { return kk; }
long MK::ex() const { return kk; }
MFar::MFar(int f) : MBase(f), MBoth(f + 1, f + 2, f + 3, f + 4), far(f + 5) { extra[0] = 'm'; }

int main() {
    // The layouts agree.
    printf("sizes %zu %zu %zu %zu\n", sizeof(MLeft), sizeof(MRight), sizeof(MBoth), sizeof(MFar));

    // mlx-cc's objects read by g++'s code.
    MBoth mine(1, 2, 3, 4);
    printf("mine: left %ld right %ld\n", gReadLeft(&mine), gReadRight(mine));
    gSetBase(&mine, 50);
    printf("mine after g++ set: base %d right %ld via %p\n", mine.base, mine.right, (void *)(gBaseOf(&mine) == static_cast<MBase *>(&mine)));
    MFar far(10);
    printf("far: %ld\n", gReadFar(&far));
    MLeft alone(7);
    printf("alone: %ld\n", gReadLeft(&alone));

    // g++'s objects read by mlx-cc's code.
    GBoth *theirs = gMakeBoth(20);
    GLeft *left = theirs;
    GRight *right = theirs;
    GBase *base = right;
    printf("theirs: base %d left %d right %ld both %d sum %d twice %d same %d\n", left->base, left->left, right->right, theirs->both, left->sum(), base->twice(), static_cast<GBase *>(left) == base);
    GFar *gfar = gMakeFar(30);
    printf("gfar: %d %d %ld %d %d %c\n", gfar->base, gfar->left, gfar->right, gfar->both, gfar->far, gfar->extra[0]);

    // mlx-cc's code making g++'s classes (their C1), g++'s code making
    // mlx-cc's (ours).
    GBoth *made = new GBoth(5, 6, 7, 8);
    printf("made: %d %d %ld %d %d\n", made->base, made->left, made->right, made->both, made->sum());
    MBoth *byThem = gMakeMBoth(3);
    printf("by them: %ld %ld\n", gReadLeft(byThem), gReadRight(*byThem));
    // A nearly empty virtual base as the primary one: the ABI's vcall
    // offset before the offsets of the virtual bases.
    MPrim prim(4);
    GPrim *gprim = gMakePrim(6);
    printf("prim: %ld sizes %zu %zu, theirs %d %d\n", gReadPrim(&prim), sizeof(MPrim), sizeof(GPrim), gprim->base, gprim->prim);
    // Virtual functions over the virtual bases.
    MK mk;
    const MR *mr = &mk;
    GK *gk = gMakeK();
    const GR *gr = gk;
    const GV *gv = gk;
    printf("functions: %ld %ld, theirs %d %d %d %d %d %ld %d, %d %d\n", gReadK(&mk), gReadR(mr), gk->v, gk->l, gk->r, gk->j, gk->kk, gk->x, gr->r, gr->v, gv->v);
    delete made;
    delete theirs;
    delete gfar;
    delete byThem;
    return 0;
}
