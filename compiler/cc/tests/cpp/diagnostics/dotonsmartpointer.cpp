// A member reached with `.` through a smart pointer (WebCore's
// `blobRegistry()` gives a CheckedRef): the message names the member, the
// class, and the `->` it has.
struct Registry { int* impl() { return nullptr; } };
template<typename T> struct Checked {
    T* pointer;
    T* operator->() const { return pointer; }
    operator T&() const { return *pointer; }
};
Checked<Registry> registry();
int* use() { return registry().impl(); }
