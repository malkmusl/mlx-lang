// What the two units of the program test share: a record with a method,
// a template, an inline function, a class with virtual functions, and
// what one unit defines for the other.
#pragma once
#include <map>
#include <vector>

namespace program {

struct Point {
    int x;
    int y;
    int sum() const { return x + y; }
};

template <class T> struct Box {
    T value;
    T get() const { return value; }
    void set(T v) { value = v; }
};

inline int twice(int v) { return v * 2; }

class Shape {
  public:
    virtual ~Shape() {}
    virtual int area() const = 0;
    int id = 0;
};

// (shapes.cpp)
Shape *makeSquare(int side);
int total(const std::vector<Point> &points);
int lookup(const std::map<int, Box<int>> &table, int key);
extern int gMade;

}
