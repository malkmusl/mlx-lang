// The translator's test of: see tools/check_lattice_emit.sh (hides).
namespace hides {
struct Base
{
    unsigned count = 3;
    unsigned mark = 5;
    unsigned baseSum() const { return count * 10 + mark; }
    virtual ~Base() {}
    virtual unsigned weigh() const { return count; }
};
struct Middle : Base
{
    unsigned count = 40;
    unsigned middleSum() const { return count + Base::count * 100; }
    unsigned weigh() const override { return count + mark; }
};
struct Top : Middle
{
    unsigned count = 700;
    unsigned mark = 9000;
    unsigned topSum() const { return count + Middle::count + Base::count + mark + Base::mark; }
    unsigned weigh() const override { return count * 2; }
};
unsigned run()
{
    Top top;
    top.count += 1;
    top.Middle::count += 2;
    top.Base::count += 4;
    Base *base = &top;
    Middle *middle = &top;
    unsigned total = top.topSum() + top.middleSum() * 3 + top.baseSum() * 7;
    total += base->weigh() + base->count + middle->count * 5 + middle->weigh();
    Middle plain;
    total += plain.weigh() + plain.baseSum();
    return total;
}
}
