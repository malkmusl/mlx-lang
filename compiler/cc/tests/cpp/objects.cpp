// More of classes: nested classes, enums and typedefs in a class,
// friends, operator<< on a stream-like class, static constexpr members,
// member default initializers, arrays of objects, placement new,
// pointers and references to a base, temporaries in conditions, a class
// returned through several paths.
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <new>

void *operator new(unsigned long n) { return malloc(n); }
void *operator new[](unsigned long n) { return malloc(n); }
void operator delete(void *p) { free(p); }
void operator delete[](void *p) { free(p); }

class Out {
public:
    Out &operator<<(const char *text) { fputs(text, stdout); return *this; }
    Out &operator<<(int value) { printf("%d", value); return *this; }
    Out &operator<<(double value) { printf("%.2f", value); return *this; }
    Out &operator<<(char c) { putchar(c); return *this; }
};

static Out out;

struct Tracked {
    static int count;
    int id;
    Tracked() : id(++count) { out << "Tracked " << id << " made\n"; }
    Tracked(const Tracked &o) : id(++count) { out << "Tracked " << id << " copied from " << o.id << '\n'; }
    ~Tracked() { out << "Tracked " << id << " gone\n"; }
};
int Tracked::count = 0;

class Matrix {
public:
    typedef double Element;
    enum Order { ROW, COLUMN };
    static constexpr int SIZE = 2;
    struct Cell {
        int row, column;
        Element value;
        bool diagonal() const { return row == column; }
    };
    Matrix(Order o = ROW) : order(o) {
        for (int i = 0; i < SIZE * SIZE; i++) cells[i] = 0;
    }
    Element &at(int r, int c) { return cells[order == ROW ? r * SIZE + c : c * SIZE + r]; }
    Element at(int r, int c) const { return cells[order == ROW ? r * SIZE + c : c * SIZE + r]; }
    Cell cell(int r, int c) const { Cell x = {r, c, at(r, c)}; return x; }
    Matrix operator*(const Matrix &o) const {
        Matrix m(order);
        for (int r = 0; r < SIZE; r++)
            for (int c = 0; c < SIZE; c++) {
                Element sum = 0;
                for (int k = 0; k < SIZE; k++) sum += at(r, k) * o.at(k, c);
                m.at(r, c) = sum;
            }
        return m;
    }
    friend Out &operator<<(Out &o, const Matrix &m);
    friend bool same(const Matrix &a, const Matrix &b) {
        for (int i = 0; i < SIZE * SIZE; i++) if (a.cells[i] != b.cells[i]) return false;
        return true;
    }
private:
    Element cells[SIZE * SIZE];
    Order order;
};

Out &operator<<(Out &o, const Matrix &m) {
    for (int r = 0; r < Matrix::SIZE; r++) {
        o << "[";
        for (int c = 0; c < Matrix::SIZE; c++) o << ' ' << m.at(r, c);
        o << " ]\n";
    }
    return o;
}

struct Animal {
    const char *name;
    int legs = 4;
    Animal(const char *n) : name(n) {}
    virtual ~Animal() {}
    virtual const char *sound() const { return "..."; }
    void speak() const { out << name << " says " << sound() << " on " << legs << " legs\n"; }
};

struct Bird : Animal {
    Bird(const char *n) : Animal(n) { legs = 2; }
    const char *sound() const { return "tweet"; }
};

struct Dog : Animal {
    Dog() : Animal("dog") {}
    const char *sound() const { return "woof"; }
};

void speakAll(const Animal &a) { a.speak(); }
int legsOf(Animal *a) { return a->legs; }

Tracked pick(bool first) {
    Tracked a;
    if (first) {
        Tracked b;
        a.id += 100;
        return a;
    }
    return a;
}

struct Pair {
    Tracked left;
    Tracked right;
};

int main() {
    out << "start\n";
    {
        Tracked arr[2];
        Tracked copy = arr[1];
        out << "array made, count " << Tracked::count << '\n';
    }
    out << "after array, count " << Tracked::count << '\n';

    Matrix a, b(Matrix::COLUMN);
    a.at(0, 0) = 1; a.at(0, 1) = 2; a.at(1, 0) = 3; a.at(1, 1) = 4;
    b.at(0, 0) = 5; b.at(0, 1) = 6; b.at(1, 0) = 7; b.at(1, 1) = 8;
    Matrix c = a * b;
    out << c;
    Matrix::Cell cell = c.cell(1, 1);
    out << "cell " << cell.row << ' ' << cell.column << ' ' << cell.value << " diagonal " << (cell.diagonal() ? "yes" : "no") << '\n';
    out << "same " << (same(a, a) ? 1 : 0) << (same(a, b) ? 1 : 0) << '\n';

    Bird bird("sparrow");
    Dog dog;
    speakAll(bird);
    speakAll(dog);
    Animal *animals[2] = { &bird, &dog };
    for (int i = 0; i < 2; i++) out << animals[i]->name << ": " << legsOf(animals[i]) << '\n';
    Animal &ref = dog;
    out << "ref " << ref.sound() << '\n';

    char storage[sizeof(Tracked)];
    Tracked *placed = new (storage) Tracked();
    out << "placed " << placed->id << '\n';
    placed->~Tracked();

    Tracked picked = pick(false);
    out << "picked " << picked.id << '\n';
    if (pick(true).id > 0) out << "temporary in condition\n";
    out << "after condition\n";

    Pair *pair = new Pair;
    out << "pair " << pair->left.id << ' ' << pair->right.id << '\n';
    delete pair;

    Tracked *many = new Tracked[2];
    delete[] many;
    out << "end, count " << Tracked::count << '\n';
    return 0;
}
