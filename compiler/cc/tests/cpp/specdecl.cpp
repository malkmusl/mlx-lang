// An explicit specialization declared only names its result type
// without making the class (Converter<int> is not specialized yet).
#include <cstdio>

template<typename T> struct Converter;
template<typename T> struct Result {
    using ReturnType = typename Converter<T>::ReturnType;
    ReturnType value;
};

template<typename T> Result<T> convert(int);
template<> Result<int> convert<int>(int);
template<> Result<long> convert<long>(int);

template<> struct Converter<int> { using ReturnType = int; };
template<> struct Converter<long> { using ReturnType = long; };

template<> Result<int> convert<int>(int x) { return { x * 2 }; }
template<> Result<long> convert<long>(int x) { return { x * 3L }; }

int main(int argc, char** argv)
{
    std::printf("%d %ld\n", convert<int>(argc).value, convert<long>(argc).value);
    return 0;
}
