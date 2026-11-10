// The translator's test of: see tools/check_lattice_emit.sh (members).
namespace members {
struct Machine
{
    typedef unsigned (Machine::*Handler)(unsigned);
    unsigned total = 0;
    unsigned scale = 3;
    Handler handlers[4];
    unsigned Machine::*field = &Machine::scale;
    unsigned add(unsigned n) { total += n; return total; }
    unsigned times(unsigned n) { total *= n; return total; }
    unsigned scaled(unsigned n) { total += n * scale; return total; }
    virtual unsigned custom(unsigned n) { total += n + 1; return total; }
    virtual ~Machine() {}
    Machine()
    {
        handlers[0] = &Machine::add;
        handlers[1] = &Machine::times;
        handlers[2] = &Machine::scaled;
        handlers[3] = &Machine::custom;
    }
    unsigned run(unsigned which, unsigned n) { return (this->*handlers[which])(n); }
};
struct Loud : Machine
{
    unsigned custom(unsigned n) override { total += n * 100; return total; }
};
static unsigned apply(Machine &machine, unsigned (Machine::*how)(unsigned), unsigned n) { return (machine.*how)(n); }
unsigned run()
{
    Machine machine;
    // (One call a statement: the order of a sum's operands is the
    // compiler's choice.)
    unsigned total = machine.run(0, 5);
    total += machine.run(1, 4);
    total += machine.run(2, 2);
    total += machine.run(3, 7);
    Loud loud;
    total += loud.run(3, 2);
    total += apply(loud, &Machine::add, 9);
    total += apply(loud, &Machine::custom, 1);
    total += machine.*(machine.field);
    machine.*(machine.field) = 10;
    total += machine.run(2, 1);
    return total;
}
}
