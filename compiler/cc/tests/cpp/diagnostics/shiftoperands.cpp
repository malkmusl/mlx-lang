// A stream read into a float with no operator>> at all (mlxlibcpp's
// std::stringstream was an ostringstream): the built-in `>>` does not
// take what the stream converts to (its operator bool); the message names
// the operands' types, as C++'s does.
struct Stream {
    operator bool() const { return true; }
};
float read(Stream& stream)
{
    float value = 0;
    stream >> value;
    return value;
}

// (Inside its braces a scoped enum's enumerators are of its underlying
// type: shifted, no error.)
enum class Property : unsigned long long {
    First = 1ULL << 0,
    Second = First << 1,
    Third = Second << 1,
};
