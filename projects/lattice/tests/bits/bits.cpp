// The bit-fields test: bit-fields in words of their own beside other
// members, an anonymous union, two base classes each with a table pointer
// and bit-fields. `run` is translated (bits.mlx) and compared with the C++.
namespace bits {

struct Flags {
    unsigned a : 3;
    unsigned b : 13;
    bool on : 1;
    int s : 5;
    unsigned char tail;
    unsigned long long wide : 40;
    unsigned long long more : 30;
    unsigned x : 9;
};

struct Many {
    unsigned long long f0 : 1;
    unsigned long long f1 : 1;
    unsigned long long big : 60;
    unsigned long long g0 : 7;
    unsigned long long g1 : 50;
    unsigned long long h : 33;
    int plain;
};

struct Value {
    int kind;
    union {
        int i;
        float f;
        struct {
            short lo;
            short hi;
        };
    };
};

struct Left {
    virtual ~Left() {}
    virtual int left() const { return 1; }
    unsigned lbits : 4;
};

struct Right {
    virtual ~Right() {}
    virtual int right() const { return 2; }
    unsigned rbits : 6;
    int r;
};

struct Both : Left, Right {
    int left() const override { return 10 + lbits; }
    int right() const override { return 20 + rbits + r; }
    unsigned own : 5;
};

static unsigned mix(unsigned h, unsigned long long v) {
    h ^= (unsigned)(v ^ (v >> 32));
    h *= 16777619u;
    return h;
}

int run() {
    unsigned h = 2166136261u;
    Flags f;
    f.a = 5;
    f.b = 5000;
    f.on = true;
    f.s = -7;
    f.tail = 200;
    f.wide = 0xABCDEF0123ull;
    f.more = 0x2FFFFFF1ull;
    f.x = 300;
    f.b = f.b + 3000;
    f.a = f.a + 4;
    f.s = f.s - 12;
    h = mix(h, f.a);
    h = mix(h, f.b);
    h = mix(h, f.on ? 7 : 9);
    h = mix(h, (unsigned long long)(long long)f.s);
    h = mix(h, f.tail);
    h = mix(h, f.wide);
    h = mix(h, f.more);
    h = mix(h, f.x);
    h = mix(h, sizeof(Flags));

    Many m;
    m.f0 = 1;
    m.f1 = 0;
    m.big = 0x0FEDCBA987654321ull;
    m.g0 = 100;
    m.g1 = 0x3FFFFFFFFFFFFull - 5;
    m.h = 0x1DEADBEEFull;
    m.plain = -3;
    h = mix(h, m.f0 + 2 * m.f1);
    h = mix(h, m.big);
    h = mix(h, m.g0);
    h = mix(h, m.g1);
    h = mix(h, m.h);
    h = mix(h, (unsigned)m.plain);
    h = mix(h, sizeof(Many));

    Value v;
    v.kind = 2;
    v.i = 0x12345678;
    h = mix(h, (unsigned)v.lo);
    h = mix(h, (unsigned)v.hi);
    v.hi = 7;
    h = mix(h, (unsigned)v.i);

    Both both;
    both.lbits = 9;
    both.rbits = 33;
    both.r = 4;
    both.own = 21;
    Left *l = &both;
    Right *r = &both;
    h = mix(h, l->left());
    h = mix(h, r->right());
    h = mix(h, both.own);
    h = mix(h, sizeof(Both));
    return (int)h;
}

}
