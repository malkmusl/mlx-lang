// `new` of a class that leaves a pure virtual function of its base
// (WebCore's MediaPlayerPrivateMlx without the ref() and deref() of
// AbstractRefCounted): refused, naming the class and the function,
// rather than a vtable with an empty slot that the first call falls
// into.
struct Counted {
    virtual void ref() const = 0;
    virtual void deref() const = 0;
    virtual ~Counted() = default;
};
struct Player : Counted {
    void deref() const override { }
    int plays = 0;
};
Counted* make()
{
    return new Player;
}
