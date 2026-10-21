// Implicit deduction guides of constructors naming the class's member
// types: an enum of the class (`CheckedRef { ptr, CheckedRef<T>::Adopt }`
// in WTF's CheckedPtr::releaseNonNull), and an alias naming the class's
// own members (EnumSet's `using StorageType = Sized<storageSize()>::Type`),
// both named through the class.
#include <cstdio>
#include <utility>
template<typename T> struct Raw { };
template<typename T, typename PtrTraits = Raw<T>> class CheckedRef {
public:
    CheckedRef(T& object) : m_ptr(&object) { }
    enum AdoptTag { Adopt };
    CheckedRef(T& object, AdoptTag) : m_ptr(&object) { }
    T* m_ptr;
};
template<typename T, typename PtrTraits = Raw<T>> class CheckedPtr {
public:
    CheckedPtr(T* p) : m_ptr(p) { }
    CheckedRef<T> releaseNonNull()
    {
        auto& ptr = *std::exchange(m_ptr, nullptr);
        return CheckedRef { ptr, CheckedRef<T>::Adopt };
    }
    T* m_ptr;
};
template<int N> struct Sized { using Type = unsigned; };
template<> struct Sized<8> { using Type = unsigned long; };
template<typename E> class Set {
public:
    static constexpr int storageSize() { return sizeof(E) * 2; }
    using StorageType = typename Sized<storageSize()>::Type;
    constexpr Set(E e) : m_storage(1u << static_cast<int>(e)) { }
    constexpr Set(StorageType raw, int) : m_storage(raw) { }
    StorageType m_storage;
};
enum class Kind { A, B, C };
struct F { int v = 3; };
int main() {
    F f;
    CheckedPtr<const F> p(&f);
    auto r = p.releaseNonNull();
    printf("%d\n", r.m_ptr->v);
    CheckedPtr<F> q(&f);
    printf("%d\n", q.releaseNonNull().m_ptr->v);
    Set s { Kind::C };
    printf("%u %d\n", (unsigned)s.m_storage, (int)sizeof(s.m_storage));
}
