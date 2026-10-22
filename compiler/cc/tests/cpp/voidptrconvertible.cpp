// std::is_convertible of `void *` to a pointer to an object is false in
// C++ (WTF's CrossThreadCopier<void> took the RefPtr<void> copier for
// ThreadSafeRefCounted pointers, NativePromise<void, E> a Result of
// Expected<RefPtr<void>, E>).
#include <cstdio>
#include <type_traits>
struct Base { };
struct Derived : Base { };
template<typename T> struct RemovePointer { typedef T Type; };
template<typename T> struct RemovePointer<T*> { typedef T Type; };
template<typename T> struct IsCountedPointer {
    static constexpr bool value = std::is_convertible<typename RemovePointer<T>::Type*, Base*>::value;
};
template<bool counted, typename T> struct CopierBase { using Type = T; };
template<typename T> struct CopierBase<true, T> { using Type = typename RemovePointer<T>::Type*; };
template<> struct CopierBase<false, void> { using Type = void; };
template<typename T> struct Copier : CopierBase<IsCountedPointer<T>::value, T> { };
int main() {
    printf("%d %d %d %d\n", std::is_convertible_v<void*, Base*>, std::is_convertible_v<Derived*, void*>, std::is_convertible_v<Derived*, Base*>, std::is_convertible_v<const void*, void*>);
    printf("%d %d %d\n", IsCountedPointer<void>::value, IsCountedPointer<Derived*>::value, (int)std::is_void_v<Copier<void>::Type>);
    printf("%d\n", (int)std::is_same_v<Copier<Derived*>::Type, Derived*>);
}
