// The g++ side of vbaselink (see shared.h).
#include "shared.h"

GBase::GBase(int b) : base(b) {}
int GBase::twice() const { return base * 2; }
GLeft::GLeft(int l) : GBase(-1), left(l) {}
int GLeft::sum() const { return base + left; }
GRight::GRight(long r) : GBase(-2), right(r) {}
GBoth::GBoth(int b, int l, long r, short s) : GBase(b), GLeft(l), GRight(r), both(s) {}
GFar::GFar(int f) : GBase(f), GBoth(f + 1, f + 2, f + 3, f + 4), far(f + 5) { extra[0] = 'g'; }

GBoth *gMakeBoth(int k) { return new GBoth(k, k + 1, k + 2, k + 3); }
GFar *gMakeFar(int k) { return new GFar(k); }
MBoth *gMakeMBoth(int k) { return new MBoth(k, k * 2, k * 3, k * 4); }

long gReadLeft(const MLeft *l) { return l->base * 1000 + l->left * 10 + l->sum() + l->twice(); }
long gReadRight(const MRight &r) { return r.base * 1000 + r.right; }
void gSetBase(MBoth *b, int value) { b->base = value; static_cast<MRight *>(b)->right += value; }
long gReadFar(const MFar *f) { return f->base * 100000 + f->left * 1000 + f->right * 100 + f->both * 10 + f->far + f->extra[0]; }
const MBase *gBaseOf(const MRight *r) { return r; }

int GNearly::nearly() const { return 1; }
void GNearly::other() {}
GPrim::GPrim(int p) : GBase(p * 2), prim(p) {}
int GPrim::nearly() const { return prim; }
GPrim *gMakePrim(int k) { return new GPrim(k); }
long gReadPrim(const MPrim *p) { return p->base * 100 + p->prim; }

GV::GV() : v(1) {}
int GV::f() const { return v; }
int GV::g() const { return v; }
GV::~GV() {}
GL::GL() : l(2) {}
int GL::f() const { return l; }
int GL::h() const { return l; }
GR::GR() : r(3) {}
int GR::g() const { return r; }
int GR::k() const { return r; }
GJ::GJ() : j(4) {}
int GJ::h() const { return j; }
GX::GX() : x(5) {}
long GX::ex() const { return x; }
GX::~GX() {}
GK::GK() : kk(6) { v = 10; l = 20; r = 30; j = 40; x = 50; }
int GK::f() const { return kk; }
long GK::ex() const { return kk; }
GK *gMakeK() { return new GK; }
long gReadK(const MK *k) { return k->v * 100000L + k->l * 10000L + k->r * 1000L + k->j * 100L + k->x * 10L + k->kk; }
long gReadR(const MR *r) { return r->v * 10L + r->r; }
