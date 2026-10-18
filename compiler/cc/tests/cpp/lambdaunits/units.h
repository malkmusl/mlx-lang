// Lambdas of two files, each the first of its file in the same class,
// put into the same template (WTF's Function and its CallableWrapper, in
// small): each file's instance of the template must call its own lambda,
// though the template's instances are linked once for the program.
#include <stdio.h>

struct WrapperBase {
    virtual ~WrapperBase() { }
    virtual int call() = 0;
};

template<typename C> struct Wrapper final : WrapperBase {
    explicit Wrapper(C c) : callable(c) { }
    int call() final { return callable(); }
    C callable;
};

template<typename C> WrapperBase* wrap(C c) { return new Wrapper<C>(c); }

struct Queue {
    int fromA();
    int fromB();
};
