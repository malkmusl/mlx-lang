// A member template's result type names a class whose instance reads
// the class being defined (WebCore's Converter<IDLArrayBuffer> deriving
// from TypedArrayConverter, whose convert<E>() returns
// ConversionResult<IDL>, which reads Converter<IDL>::ReturnType).
#include <cstdio>

template<typename T> struct Converter;
template<typename T> struct Result {
    using ReturnType = typename Converter<T>::ReturnType;
    ReturnType value;
};

struct Buffer { int size; };

template<typename T> struct Base {
    using ReturnType = int;
    template<typename E = int> static Result<T> convert(int x) { return { x * 2 }; }
};

template<> struct Converter<Buffer> : Base<Buffer> { };

int main(int argc, char** argv)
{
    std::printf("%d\n", Converter<Buffer>::convert(argc).value);
    return 0;
}
