// A stream with an operator>> for an int only, read into a float: the
// member does not take the float (an int & binds no float), and the
// built-in >> does not take the stream: an error naming the types.
struct Stream {
    Stream& operator>>(int&) { return *this; }
};
float read(Stream& stream)
{
    float value = 0;
    stream >> value;
    return value;
}
