// The translator's test of: see tools/check_lattice_emit.sh (statics).
namespace statics {
struct Name { const char *text; unsigned long length; };
struct Empty {};
class Symbol
{
  public:
    constexpr Symbol(const char *name, unsigned long length, unsigned id, unsigned kind, bool built, int level)
        : mName{name, length}, mId(id), mExtensions{1, 2, 3}, mKind(kind), mBuilt(built), mLevel(level), mWide(id * 3)
    {}
    Name mName;
    unsigned mId;
    unsigned char mExtensions[3];
    unsigned mKind : 4;
    bool mBuilt : 1;
    int mLevel : 3;
    unsigned mWide : 20;
};
class Function : public Symbol
{
  public:
    constexpr Function(const char *name, unsigned long length, unsigned id, const unsigned *parameters, unsigned count)
        : Symbol(name, length, id, 9, true, -2), mParameters(parameters), mCount(count), mKnown(true)
    {}
    const unsigned *mParameters;
    unsigned mCount : 6;
    bool mKnown : 1;
};
struct Shape
{
    virtual ~Shape() {}
    virtual unsigned area() const { return side * side; }
    unsigned side = 4;
};
constexpr Empty kEmpty{};
constexpr unsigned kParameters[6] = {11, 22, 33, 44, 55, 66};
constexpr Symbol kPlain("plain", 5, 17, 6, false, 3);
constexpr Function kFirst("first", 5, 100, kParameters, 2);
constexpr Function kSecond("second", 6, 200, &kParameters[3], 3);
constexpr const Function *kTable[2] = {&kFirst, &kSecond};
static Shape gShape;
static unsigned counter(unsigned step)
{
    // (Two static locals, each made once behind a guard of its own.)
    static Shape first;
    static unsigned second = step * 7;
    first.side += step;
    second += 1;
    return first.side * 100 + second;
}
static const Name kNames[2] = {{"alpha", 5}, {"beta", 4}};
static unsigned weigh(const Symbol &symbol)
{
    return symbol.mId + symbol.mKind * 2 + (symbol.mBuilt ? 100 : 0) + (unsigned)(symbol.mLevel + 8) + symbol.mWide + symbol.mExtensions[2] + (unsigned char)symbol.mName.text[1] + (unsigned)symbol.mName.length;
}
unsigned run()
{
    unsigned total = weigh(kPlain);
    for (const Function *function : kTable)
    {
        total = total * 3 + weigh(*function) + (function->mKnown ? 1 : 0);
        for (unsigned i = 0; i < function->mCount; i++) total += function->mParameters[i];
    }
    total += gShape.area() + (unsigned char)kNames[1].text[0] + (unsigned)kNames[0].length + (unsigned)sizeof(kEmpty);
    total += counter(2);
    total += counter(5);
    return total;
}
}
