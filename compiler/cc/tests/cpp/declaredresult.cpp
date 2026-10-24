// A function declared (not defined) names its result's class template
// instance without making it: the instance reads a specialization that
// comes later (WebCore's `ConversionResult<IDLByteString>
// valueToByteString(...);` before `Converter<IDLByteString>`).
#include <stdio.h>

template<class IDL> struct Converter;
template<class IDL> struct ConversionResult {
    using ReturnType = typename Converter<IDL>::ReturnType;
    ReturnType value;
};

struct IDLByteString { };
struct IDLLong { };

ConversionResult<IDLByteString> valueToByteString(int);
ConversionResult<IDLLong> valueToLong(int, const char *);

template<> struct Converter<IDLByteString> { using ReturnType = const char *; };
template<> struct Converter<IDLLong> { using ReturnType = long; };

ConversionResult<IDLByteString> valueToByteString(int which) { return { which ? "yes" : "no" }; }

int main(int argc, char **argv)
{
    printf("%s %ld\n", valueToByteString(argc).value, valueToLong(argc + 1, "").value);
    return argc - 2;
}

ConversionResult<IDLLong> valueToLong(int base, const char *) { return { base * 2L }; }
