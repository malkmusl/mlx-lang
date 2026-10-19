// Template value parameters that are a reference to an object and a
// pointer to a data member (WebCore's SVGPropertyOwnerRegistry:
// `template<const LazyNeverDestroyed<const QualifiedName>& attributeName,
// const Ref<SVGStringList> SVGConditionalProcessingAttributes::*property>`).
#include <stdio.h>

template<class T> struct Lazy { T value; const T &get() const { return value; } };
struct QualifiedName { const char *local; };
struct List { int count; };
struct Attributes { List requiredExtensions; List systemLanguage; };

template<const List Attributes::*property> struct Accessor {
    static int count(const Attributes &attributes) { return (attributes.*property).count; }
};

template<class Owner> struct Registry {
    template<const Lazy<const QualifiedName> &attributeName, const List Attributes::*property>
    static int describe(const Attributes &attributes)
    {
        printf("%s ", attributeName.get().local);
        return Accessor<property>::count(attributes);
    }
};

Lazy<const QualifiedName> requiredExtensionsAttr { { "requiredExtensions" } };
Lazy<const QualifiedName> systemLanguageAttr { { "systemLanguage" } };

struct Element { };

int main(int argc, char **argv)
{
    Attributes attributes { { 2 }, { argc + 4 } };
    int a = Registry<Element>::describe<requiredExtensionsAttr, &Attributes::requiredExtensions>(attributes);
    int b = Registry<Element>::describe<systemLanguageAttr, &Attributes::systemLanguage>(attributes);
    printf("%d %d\n", a, b);
    return argc - 2;
}
