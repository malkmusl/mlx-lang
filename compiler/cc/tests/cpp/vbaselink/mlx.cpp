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
    delete made;
    delete theirs;
    delete gfar;
    delete byThem;
    return 0;
}
