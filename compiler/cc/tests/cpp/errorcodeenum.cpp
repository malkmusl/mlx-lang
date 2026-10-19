// A std::error_code is made from an enum the program declares an error
// code enum (is_error_code_enum true), through the enum's own
// make_error_code: WebKit's ContentExtensionError returned where an
// Expected<T, std::error_code> is.
#include <stdio.h>
#include <string>
#include <system_error>

namespace Content {
enum class ExtensionError { None, JSONInvalid, TooMany };

class ExtensionCategory : public std::error_category {
public:
    const char *name() const noexcept override { return "content extension"; }
    std::string message(int value) const override { return value == 1 ? "invalid JSON" : "other"; }
};

inline const std::error_category &extensionCategory()
{
    static ExtensionCategory category;
    return category;
}

inline std::error_code make_error_code(ExtensionError error) { return { static_cast<int>(error), extensionCategory() }; }
}

namespace std {
template<> struct is_error_code_enum<Content::ExtensionError> : public true_type { };
}

static std::error_code parse(int input)
{
    if (input < 0)
        return Content::ExtensionError::JSONInvalid;
    if (input > 9)
        return Content::ExtensionError::TooMany;
    return { };
}

int main(int argc, char **)
{
    std::error_code a = parse(-1);
    std::error_code b = parse(20);
    std::error_code c = parse(3);
    std::error_code d;
    d = Content::ExtensionError::JSONInvalid;
    printf("%d %s %s | %d %d | %d %d\n", a.value(), a.category().name(), a.message().c_str(), b.value(), (int)(b == a), (int)(bool)c, (int)(d == a));
    return argc;
}
