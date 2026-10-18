// Classes: member functions, this, constructors and destructors,
// initializer lists, static members, operators, temporaries, new and
// delete, inheritance and virtual functions.
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// The allocation functions new and delete call (mlxlibcpp will have them).
void *operator new(unsigned long n) { return malloc(n); }
void *operator new[](unsigned long n) { return malloc(n); }
void operator delete(void *p) { free(p); }
void operator delete[](void *p) { free(p); }

static int live = 0;

class Counter {
public:
    Counter() : value(0), step(1) { live++; }
    explicit Counter(int start, int by = 1) : value(start), step(by) { live++; }
    ~Counter() { live--; }
    void bump() { value += step; }
    int get() const { return value; }
    static int alive() { return live; }
    Counter &operator++() { value += step; return *this; }
    Counter operator+(int n) const { return Counter(value + n, step); }
    bool operator==(const Counter &other) const { return value == other.value; }
private:
    int value;
    int step;
};

struct Point {
    int x, y;
    int sum() const { return x + y; }
    Point scaled(int k) const { Point p = {x * k, y * k}; return p; }
};

bool operator<(const Point &a, const Point &b) { return a.sum() < b.sum(); }

class Buffer {
public:
    Buffer(const char *text) {
        length = strlen(text);
        data = (char *)malloc(length + 1);
        memcpy(data, text, length + 1);
        copies = 0;
        printf("Buffer(\"%s\")\n", text);
    }
    Buffer(const Buffer &other) {
        length = other.length;
        data = (char *)malloc(length + 1);
        memcpy(data, other.data, length + 1);
        copies = other.copies + 1;
        printf("Buffer copy of \"%s\"\n", other.data);
    }
    ~Buffer() { printf("~Buffer(\"%s\")\n", data); free(data); }
    Buffer &operator=(const Buffer &other) {
        if (this != &other) {
            free(data);
            length = other.length;
            data = (char *)malloc(length + 1);
            memcpy(data, other.data, length + 1);
        }
        return *this;
    }
    const char *c_str() const { return data; }
    size_t size() const { return length; }
    char &operator[](size_t index) { return data[index]; }
    Buffer &operator+=(const char *more) {
        size_t extra = strlen(more);
        char *grown = (char *)malloc(length + extra + 1);
        memcpy(grown, data, length);
        memcpy(grown + length, more, extra + 1);
        free(data);
        data = grown;
        length += extra;
        return *this;
    }
    int copies;
private:
    char *data;
    size_t length;
};

Buffer make(const char *text) {
    Buffer b(text);
    b += "!";
    return b;
}

void takeByValue(Buffer b) { printf("by value: %s (copies %d)\n", b.c_str(), b.copies); }
void takeByRef(const Buffer &b) { printf("by ref: %s (copies %d)\n", b.c_str(), b.copies); }

class Shape {
public:
    Shape(const char *n) : name(n) { printf("Shape(%s)\n", n); }
    virtual ~Shape() { printf("~Shape(%s)\n", name); }
    virtual double area() const = 0;
    virtual const char *kind() const { return "shape"; }
    void describe() const { printf("%s %s: %.2f\n", kind(), name, area()); }
protected:
    const char *name;
};

class Rect : public Shape {
public:
    Rect(double w, double h) : Shape("rect"), width(w), height(h) {}
    ~Rect() { printf("~Rect\n"); }
    double area() const { return width * height; }
    const char *kind() const override { return "rectangle"; }
private:
    double width, height;
};

class Square : public Rect {
public:
    Square(double side) : Rect(side, side) {}
    const char *kind() const override { return "square"; }
};

struct Holder {
    Buffer first;
    Buffer second;
    int tag = 7;
    Holder() : first("one"), second("two") {}
};

namespace geometry {
    struct Vec {
        double x, y;
        Vec(double a, double b) : x(a), y(b) {}
        Vec operator+(const Vec &o) const { return Vec(x + o.x, y + o.y); }
        double dot(const Vec &o) const;
        static int made;
    };
    int Vec::made = 0;
    double Vec::dot(const Vec &o) const { made++; return x * o.x + y * o.y; }
}

int main() {
    Counter c;
    c.bump();
    c.bump();
    ++c;
    printf("counter %d alive %d\n", c.get(), Counter::alive());
    {
        Counter d(10, 5);
        Counter e = d + 3;
        printf("d %d e %d alive %d equal %d\n", d.get(), e.get(), Counter::alive(), d == Counter(10));
    }
    printf("alive after block %d\n", Counter::alive());

    Point p = {3, 4};
    Point q = p.scaled(2);
    printf("point %d %d sum %d less %d\n", q.x, q.y, q.sum(), p < q);

    Buffer b("hello");
    b += " world";
    b[0] = 'H';
    printf("%s (%zu)\n", b.c_str(), b.size());
    Buffer copy = b;
    printf("copy %s copies %d\n", copy.c_str(), copy.copies);
    Buffer m = make("made");
    printf("made %s\n", m.c_str());
    takeByValue(b);
    takeByRef(b);
    takeByValue(make("temp"));
    copy = m;
    printf("assigned %s\n", copy.c_str());
    printf("size of temporary %zu\n", make("abc").size());

    Shape *shapes[2];
    shapes[0] = new Rect(2, 3);
    shapes[1] = new Square(4);
    for (int i = 0; i < 2; i++) shapes[i]->describe();
    const Shape &ref = *shapes[1];
    printf("ref kind %s area %.1f\n", ref.kind(), ref.area());
    for (int i = 0; i < 2; i++) delete shapes[i];

    int *numbers = new int[5];
    for (int i = 0; i < 5; i++) numbers[i] = i * i;
    printf("numbers %d %d\n", numbers[2], numbers[4]);
    delete[] numbers;
    Counter *counters = new Counter[3];
    printf("alive with array %d\n", Counter::alive());
    delete[] counters;
    printf("alive after delete[] %d\n", Counter::alive());

    {
        Holder h;
        printf("holder %s %s %d\n", h.first.c_str(), h.second.c_str(), h.tag);
    }

    geometry::Vec v(1, 2), w(3, 4);
    geometry::Vec s = v + w;
    printf("vec %.1f %.1f dot %.1f made %d\n", s.x, s.y, v.dot(w), geometry::Vec::made);

    for (int i = 0; i < 3; i++) {
        Buffer inner("loop");
        if (i == 1) continue;
        if (i == 2) break;
        printf("loop body %d\n", i);
    }
    printf("end\n");
    return 0;
}
