// A class template's instance over a function type naming a template
// parameter, in a function template's parameter (`CompletionHandler<
// void(T)> &&`), is a pattern deduced from the argument's instance: it
// was made as an instance of its own and deduced nothing (WebCore's
// PushDatabase: `completeOnMainQueue(WTF::move(completionHandler),
// result)` in a lambda in a lambda, "no instance takes the arguments").
#include <stdio.h>
#include <utility>

template<class F> struct Handler;
template<class R, class A> struct Handler<R(A)> {
    void (*f)(A);
    void operator()(A a) { f(a); }
};
template<class F> struct Plain { F *f; };

template<class T, class U> static void complete(Handler<void(T)> &&handler, U &&result) { handler(std::forward<U>(result)); }
template<class T> static int byReference(Handler<void(T)> &handler) { return sizeof(T); }
template<class R, class... A> static int arity(Plain<R(A...)> &&) { return (int)sizeof...(A) * 10 + (int)sizeof(R); }
template<class F> struct Exit {
    F f;
    ~Exit() { f(); }
};
template<class F> Exit<F> makeExit(F f) { return Exit<F> { f }; }
template<class F> void dispatch(F &&f) { f(); }

enum class Changed { No, Yes };
static void show(Changed c) { printf("changed %d\n", (int)c); }

int main(int argc, char **)
{
    Handler<void(Changed)> h { show };
    printf("%d %d\n", byReference(h), arity(Plain<long(int, char)> { nullptr }));
    dispatch([h = std::move(h)]() mutable {
        auto result = Changed::Yes;
        auto scope = makeExit([&h, &result] {
            complete(std::move(h), result);
        });
    });
    return argc;
}
