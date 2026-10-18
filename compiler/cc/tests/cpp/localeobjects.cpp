// POSIX 2008's locale objects of <locale.h> (Skia's SkAutoLocaleSetter:
// newlocale, uselocale, freelocale), in the C locale.
#include <locale.h>
#include <cstdio>

class AutoLocale {
public:
    explicit AutoLocale(const char *name) {
        fLocale = newlocale(LC_ALL_MASK, name, nullptr);
        if (fLocale) {
            fOldLocale = uselocale(fLocale);
        } else {
            fOldLocale = static_cast<locale_t>(nullptr);
        }
    }
    ~AutoLocale() {
        if (fLocale) {
            uselocale(fOldLocale);
            freelocale(fLocale);
        }
    }
    bool set() const { return fLocale != nullptr; }
private:
    locale_t fOldLocale;
    locale_t fLocale;
};

int main(int argc, char **argv) {
    {
        AutoLocale c("C");
        printf("%d %.2f\n", c.set(), 3.25);
    }
    printf("%d\n", (LC_ALL_MASK & LC_NUMERIC_MASK) != 0);
    return argc;
}
