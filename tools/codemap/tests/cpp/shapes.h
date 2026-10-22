// The C++ of mlx-codemap's check (tools/check_codemap_cpp.sh): classes,
// a template, overloads, namespaces, macros, a virtual function and its
// override, and a function an Mlx module exports (shapes_api.h).
#pragma once

#define SQUARE(x) ((x) * (x))
#define PI 3

namespace geo {

// A shape on the plane.
class Shape {
public:
    virtual ~Shape() {}
    // Its area, rounded.
    virtual int area() const = 0;
    int id;
};

// A circle of a radius.
class Circle : public Shape {
public:
    explicit Circle(int r) : radius(r) {}
    int area() const override;
    int radius;
};

// A value in a box.
template <typename T> struct Box {
    T value;
    T get() const { return value; }
    void put(T given) { value = given; }
};

// The shapes counted, by kind of argument.
int count(int shapes);
int count(const Shape &shape);

namespace detail {
int scaled(int value);
}

}
