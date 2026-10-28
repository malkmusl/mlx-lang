// An alias template's defaults name its namespace's templates (WTF's
// UncheckedKeyHashMap defaulting HashTraits<KeyArg>) when the alias is
// a pattern in a function template's signature outside that namespace.
#include <stdio.h>
namespace W {
template<typename T> struct DefaultHash { static constexpr int id = 3; };
template<typename T> struct HashTraits { static constexpr int id = 1; };
struct HashTableTraits { static constexpr int id = 2; };
struct HashTableMalloc { };
enum class ShouldValidateKey : bool { No, Yes };
template<typename KeyArg, typename MappedArg, typename = DefaultHash<KeyArg>, typename = HashTraits<KeyArg>, typename = HashTraits<MappedArg>, typename = HashTableTraits, ShouldValidateKey = ShouldValidateKey::Yes, typename = HashTableMalloc> class HashMap;
template<typename KeyArg, typename MappedArg, typename KeyHash = DefaultHash<KeyArg>, typename KeyTraits = HashTraits<KeyArg>, typename MappedTraits = HashTraits<MappedArg>, typename HashTraits = HashTableTraits, typename Malloc = HashTableMalloc>
using UncheckedKeyHashMap = HashMap<KeyArg, MappedArg, KeyHash, KeyTraits, MappedTraits, HashTraits, ShouldValidateKey::No, Malloc>;
template<typename K, typename M, typename KH, typename KT, typename MT, typename HT, ShouldValidateKey v, typename A>
class HashMap { public: static int f() { return KH::id * 1000 + KT::id * 100 + MT::id * 10 + HT::id + (v == ShouldValidateKey::No ? 0 : 5000); } };
}
namespace J { struct Desc { int x; }; struct Node; }
namespace W { template<> struct HashTraits<J::Desc> { static constexpr int id = 7; }; }
namespace J {
struct Phase {
    using Fields = W::UncheckedKeyHashMap<Desc, Node*>;
    int run() { W::UncheckedKeyHashMap<Desc, Node*> a; return Fields::f() + merge(a, a); }
    template<typename Key>
    static int merge(W::UncheckedKeyHashMap<Key, Node*>& my, const W::UncheckedKeyHashMap<Key, Node*>& their) { return 10000; }
};
}
int main() { J::Phase p; printf("%d\n", p.run()); return 0; }
