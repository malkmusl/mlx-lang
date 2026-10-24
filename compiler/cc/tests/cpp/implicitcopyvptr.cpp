// An object of a class with a vtable made by its implicit copy constructor
// gets its own class's vtable: after a base's explicit copy constructor
// (which stores the base's), and when the source is an object of a class
// derived from it (whose bytes hold the derived class's). WebCore's
// GraphicsLayerFloatAnimationValue::clone() is makeUnique of a copy, over
// GraphicsLayerAnimationValue's explicit copy constructor; the copy had the
// base's table and its own clone() was the pure virtual's empty slot.
#include <memory>
#include <stdio.h>
#include <string>

struct Value {
    virtual ~Value() = default;
    virtual std::unique_ptr<Value> clone() const = 0;
    virtual int kind() const { return 0; }
    double time;
protected:
    explicit Value(double t) : time(t) { }
    explicit Value(const Value& other) : time(other.time) { }
};

struct FloatValue : Value {
    FloatValue(double t, float v) : Value(t), value(v) { }
    std::unique_ptr<Value> clone() const override { return std::make_unique<FloatValue>(*this); }
    int kind() const override { return 1; }
    float value;
};

// (A member with a constructor: the copy is made member by member.)
struct NamedValue : Value {
    NamedValue(double t, std::string n) : Value(t), name(std::move(n)) { }
    std::unique_ptr<Value> clone() const override { return std::make_unique<NamedValue>(*this); }
    int kind() const override { return 2; }
    std::string name;
};

// (Bytes only: a plain polymorphic class sliced from a derived object.)
struct Shape {
    virtual ~Shape() = default;
    virtual int sides() const { return 0; }
    int id { 0 };
};
struct Square : Shape {
    int sides() const override { return 4; }
};

int main()
{
    FloatValue f(1.5, 2);
    auto a = f.clone();
    auto b = a->clone();
    printf("%d %d %g\n", a->kind(), b->kind(), b->time);
    NamedValue n(2.5, "named");
    auto c = n.clone();
    auto d = c->clone();
    printf("%d %d %s\n", c->kind(), d->kind(), static_cast<NamedValue&>(*d).name.c_str());
    Square square;
    square.id = 9;
    Shape sliced(square);
    Shape* pointer = &sliced;
    printf("%d %d\n", pointer->sides(), sliced.id);
    return 0;
}
