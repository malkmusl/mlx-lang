// Templates whose definitions only b.cpp holds: a.cpp names their
// instances, which b.cpp's explicit instantiation definitions make.
struct V1 { int n = 1; };
struct V2 { int n = 2; };
struct S {
    template<typename V> int visit(V&);
    int total = 0;
};
template<int N> struct C {
    int f();
    int g() { return N * 10; }
    static int s;
    struct Inner { int h(); };
    template<class T> T same(T t) { return t; }
};
template<class T> T twice(T);
namespace ns {
template<class T> struct Box {
    T value;
    T get() const;
    void set(T);
};
}
// (Declarations: they make nothing.)
extern template struct C<3>;
extern template int S::visit(V1&);
extern template struct ns::Box<long>;
