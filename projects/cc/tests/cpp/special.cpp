// Explicit specializations: a class template for one argument, a
// function template for one argument (named, or deduced).
#include <iostream>
#include <string>
using namespace std;

template <typename T> struct Describe {
    static string name() { return "something"; }
    T value;
    string show() const { return name() + " of size " + to_string(sizeof(T)); }
};
template <> struct Describe<bool> {
    static string name() { return "a flag"; }
    bool value;
    string show() const { return value ? "set" : "clear"; }
};
template <> struct Describe<string> {
    string value;
    string show() const { return "text \"" + value + "\""; }
};

template <typename T> string kind(T) { return "other"; }
template <> string kind<int>(int) { return "int"; }
template <> string kind(double) { return "double"; }
template <typename T> T zero() { return T(); }
template <> string zero<string>() { return "empty"; }

int main() {
    Describe<int> i; i.value = 3;
    Describe<bool> b; b.value = true;
    Describe<string> s; s.value = "hi";
    Describe<char> c; c.value = 'x';
    cout << i.show() << " | " << b.show() << " | " << s.show() << " | " << c.show() << endl;
    cout << Describe<int>::name() << ' ' << Describe<bool>::name() << ' ' << sizeof(Describe<bool>) << endl;
    cout << kind(1) << ' ' << kind(2.5) << ' ' << kind('c') << ' ' << kind(string("s")) << endl;
    cout << zero<int>() << ' ' << zero<string>() << ' ' << zero<double>() << endl;
    return 0;
}
