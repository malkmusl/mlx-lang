// A partial specialization `Traits<Generic<type>>` matches Generic<B>
// while Generic<B> is being read: its base Cached<Generic<B>> names
// `Traits<Class>::collectionType` from the base clause, before the
// instance has its template and arguments recorded (WebCore's
// GenericCachedHTMLCollection and CollectionClassTraits).
#include <stdio.h>
enum class CollectionType { A, B, C };
enum class Traversal { Child, Descendant };
template<CollectionType> struct CollectionTypeTraits { static constexpr Traversal traversalType = Traversal::Descendant; };
template<> struct CollectionTypeTraits<CollectionType::B> { static constexpr Traversal traversalType = Traversal::Child; };
template<typename> struct CollectionClassTraits;

template<CollectionType type> class Generic;
template<CollectionType type> struct CollectionClassTraits<Generic<type>> {
    static constexpr CollectionType collectionType = type;
};

struct Collection { virtual ~Collection() { } int base { 1 }; };

template<typename Class> class Cached : public Collection {
public:
    static constexpr auto traversalType = CollectionTypeTraits<CollectionClassTraits<Class>::collectionType>::traversalType;
    int kind() const { return (int)traversalType; }
};

template<CollectionType type> class Generic final : public Cached<Generic<type>> {
public:
    static_assert(CollectionTypeTraits<type>::traversalType != Traversal::Child || type == CollectionType::B);
    int self() const { return (int)CollectionClassTraits<Generic>::collectionType; }
};

int main(int argc, char**)
{
    Generic<CollectionType::B> b;
    Generic<CollectionType::C> c;
    printf("%d %d %d %d\n", b.kind(), c.kind(), b.self(), c.self());
    return argc;
}
