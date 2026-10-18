// Templates: function templates deduced from their arguments or given
// theirs, class templates with type and value parameters, members
// defined outside the class, instances made inside functions, nested
// instances, templates in namespaces.
#include <string>
#include <iostream>
using namespace std;

template <typename T> T biggest(T a, T b) { return a < b ? b : a; }
template <typename T> void swapValues(T &a, T &b) { T t = a; a = b; b = t; }
template <typename T, int N> struct Box {
    T items[N];
    int count;
    Box() : count(0) {}
    void add(T item) { items[count++] = item; }
    T sum() const { T s = 0; for (int i = 0; i < count; i++) s += items[i]; return s; }
    static int capacity() { return N; }
};

namespace util {
    template <typename T> class List {
    public:
        List() : items(0), count(0), room(0) {}
        List(const List &other) : items(0), count(0), room(0) { for (int i = 0; i < other.count; i++) push_back(other.items[i]); }
        ~List() { delete[] items; }
        List &operator=(const List &other) {
            if (this != &other) {
                clear();
                for (int i = 0; i < other.count; i++) push_back(other.items[i]);
            }
            return *this;
        }
        void push_back(const T &item) {
            if (count == room) grow();
            items[count++] = item;
        }
        void clear() { delete[] items; items = 0; count = 0; room = 0; }
        T &operator[](int index) { return items[index]; }
        const T &operator[](int index) const { return items[index]; }
        int size() const { return count; }
        T *begin() { return items; }
        T *end() { return items + count; }
    private:
        T *items;
        int count, room;
        void grow();
    };
    template <typename T> void List<T>::grow() {
        room = room ? room * 2 : 4;
        T *more = new T[room];
        for (int i = 0; i < count; i++) more[i] = items[i];
        delete[] items;
        items = more;
    }
    template <typename T> T total(List<T> &list) {
        T s = T();
        for (T *p = list.begin(); p != list.end(); p++) s = s + *p;
        return s;
    }
}

template <typename T> struct Pair {
    T first, second;
    Pair(const T &a, const T &b) : first(a), second(b) {}
    bool ordered() const { return first < second; }
};
template <typename T> Pair<T> makePair(const T &a, const T &b) { return Pair<T>(a, b); }

int main() {
    cout << biggest(3, 13) << ' ' << biggest<double>(1.5, 0.5) << ' ' << biggest(string("pear"), string("apple")) << endl;
    int x = 1, y = 2;
    swapValues(x, y);
    string s = "left", t = "right";
    swapValues(s, t);
    cout << x << y << ' ' << s << ' ' << t << endl;

    Box<int, 4> b;
    b.add(5); b.add(8);
    Box<double, 2> d;
    d.add(0.5); d.add(0.25);
    cout << b.sum() << ' ' << Box<int, 4>::capacity() << ' ' << d.sum() << ' ' << sizeof(Box<int, 4>) << ' ' << sizeof(Box<double, 2>) << endl;

    util::List<int> numbers;
    for (int i = 1; i <= 5; i++) numbers.push_back(i * i);
    util::List<string> words;
    words.push_back("alpha");
    words.push_back(string("beta"));
    cout << numbers.size() << ' ' << numbers[4] << ' ' << util::total<int>(numbers) << ' ' << words.size() << ' ' << words[1] << ' ' << util::total<string>(words) << endl;
    util::List<util::List<int> > nested;
    nested.push_back(numbers);
    nested.push_back(numbers);
    nested[1][0] = 100;
    cout << nested[0][2] << ' ' << nested[1][0] << ' ' << numbers[0] << ' ' << nested.size() << endl;
    util::List<string> copy = words;
    copy.push_back("gamma");
    words = copy;
    cout << words.size() << ' ' << words[2] << ' ' << copy.size() << endl;

    Pair<int> p = makePair(4, 2);
    Pair<string> q = makePair(string("a"), string("b"));
    cout << p.first << p.second << ' ' << p.ordered() << ' ' << q.ordered() << ' ' << q.second << endl;
    return 0;
}
