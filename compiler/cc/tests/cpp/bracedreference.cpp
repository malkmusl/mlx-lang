// `return { x };` from a function returning a reference binds the
// reference to x ([dcl.init.list] 3.10): no temporary copy. WebCore's
// ShorthandSerializer::LonghandValueIterator::operator* returns
// `{ serializer.longhandValue(index) }`; with a copy, `for (Ref value :
// longhandValues())` counted references of a sliced copy on the stack
// and the serializer's loop never ended (Google's search results).
#include <cstdio>
#include <cstddef>

struct Value {
    virtual ~Value() = default;
    virtual int layers() const { return 1; }
    int refs { 1 };
};
struct List : Value { int layers() const override { return 3; } };

static Value plain;
static List list;
static Value* values[] = { &plain, &list, &plain };
static int number = 5;

static Value& get(int index) { return *values[index]; }
static Value& braced(int index) { return { get(index) }; }
static const Value& bracedConst(int index) { return { get(index) }; }
static int& bracedInt() { return { number }; }

class Serializer {
public:
    struct Iterator {
        void operator++() { ++index; }
        bool operator==(std::nullptr_t) const { return index >= 3; }
        bool operator!=(std::nullptr_t) const { return index < 3; }
        Value& operator*() const { return { get(index) }; }
        int index { 0 };
    };
    struct Range {
        Iterator begin() const { return { }; }
        static constexpr std::nullptr_t end() { return nullptr; }
    };
    Range all() const { return { }; }
};

int main()
{
    std::printf("same %d %d %d\n", &braced(1) == &list, &bracedConst(0) == &plain, &bracedInt() == &number);
    bracedInt() = 9;
    std::printf("number %d\n", number);
    int most = 0;
    int seen = 0;
    for (Value& value : Serializer().all()) {
        ++value.refs;
        if (value.layers() > most)
            most = value.layers();
        if (++seen > 10)
            break;
    }
    std::printf("layers %d seen %d refs %d %d\n", most, seen, plain.refs, list.refs);
    return 0;
}
