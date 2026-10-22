// A local alias of the function around a lambda (`using Kind =
// IntlDateTimeFormat::TemporalFieldKind;` before `[&](..., Kind kind) {
// ... Kind::PlainDate ... }` in JSC's handleDateTimeValue) names the
// enum in the lambda's body, not a base's member of the name (JSCell's
// own Kind, which a name only a using-directive finds gives way to).
#include <cstdio>
struct Cell { enum Kind { A, B }; };
struct Format : Cell {
    enum class FieldKind { None, PlainDate, PlainDateTime };
    struct Record { double value; FieldKind kind; };
    static Record handle(const Format* f, int x);
};
Format::Record Format::handle(const Format* f, int x)
{
    using Kind = Format::FieldKind;
    if (!x)
        return { 0, Kind::None };
    auto validate = [&](int id, Kind kind) -> bool {
        bool exempt = (kind == Kind::PlainDate || kind == Kind::PlainDateTime);
        return f && exempt && id;
    };
    if (validate(x, Kind::PlainDate))
        return { 1, Kind::PlainDate };
    return { 2, Kind::None };
}
int main()
{
    Format f;
    printf("%g %g\n", Format::handle(&f, 1).value, Format::handle(nullptr, 1).value);
    return 0;
}
