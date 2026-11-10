// A small hierarchy with everything ANGLE's classes use of virtual
// functions: overriding, a pure virtual, a virtual destructor, a call
// through the base, a second base (the adjustment of `this`), a call of
// the base's version, and deletion through the base.
#include <stdlib.h>
namespace shapes {
class Shape {
  public:
    Shape(int id) : mId(id) {}
    virtual ~Shape() { sDestroyed += mId; }
    virtual int area() const = 0;
    virtual int sides() const { return 0; }
    int id() const { return mId; }
    int describe() const { return area() * 100 + sides() * 10 + mId; }
    static int sDestroyed;
  protected:
    int mId;
};
int Shape::sDestroyed = 0;

class Named {
  public:
    virtual ~Named() {}
    virtual int nameLength() const { return 1; }
    int tag = 7;
};

class Square : public Shape {
  public:
    Square(int id, int side) : Shape(id), mSide(side) {}
    int area() const override { return mSide * mSide; }
    int sides() const override { return 4; }
  private:
    int mSide;
};

class Label : public Shape, public Named {
  public:
    Label(int id, int length) : Shape(id), mLength(length) {}
    int area() const override { return Shape::sides() + 1; }
    int nameLength() const override { return mLength + tag; }
  private:
    int mLength;
};

int measure(const Shape *shape) { return shape->describe(); }
int nameOf(const Named *named) { return named->nameLength(); }

int run() {
    Square square(1, 3);
    Label label(2, 5);
    int total = measure(&square) + measure(&label) * 1000;
    total += nameOf(&label) * 1000000;
    Shape *heap = new Square(4, 2);
    total += measure(heap) * 7;
    delete heap;
    return total + Shape::sDestroyed * 100000000;
}
}
