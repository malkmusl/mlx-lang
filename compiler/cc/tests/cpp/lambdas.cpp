// Lambdas: captures by copy, by reference, this, [=] and [&], deduced
// and given result types, as comparators and predicates, called at once.
#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
using namespace std;
struct Counter {
    int total;
    Counter() : total(0) {}
    void addAll(const vector<int> &v) { for_each(v.begin(), v.end(), [this](int x) { total += x; }); }
    int scaled(int k) { auto f = [=](int x) { return x * k + total; }; return f(2); }
};
int main() {
    auto twice = [](int x) { return x * 2; };
    cout << twice(21) << endl;
    int base = 10;
    auto add = [base](int x) { return x + base; };
    auto addRef = [&base](int x) { base += x; return base; };
    cout << add(5) << ' ' << addRef(5) << ' ' << base << endl;
    vector<int> v;
    for (int i = 0; i < 8; i++) v.push_back((i * 5) % 7);
    sort(v.begin(), v.end(), [](int a, int b) { return a > b; });
    for (int x : v) cout << x;
    cout << ' ' << count_if(v.begin(), v.end(), [](int x) { return x % 2 == 0; }) << endl;
    string prefix = "<";
    vector<string> words;
    words.push_back("b"); words.push_back("a");
    auto show = [&]() { for (const string &w : words) cout << prefix << w << ">"; cout << endl; };
    show();
    sort(words.begin(), words.end());
    prefix = "[";
    show();
    auto wrap = [=](const string &w) -> string { return prefix + w + ">"; };
    cout << wrap("z") << endl;
    Counter c;
    c.addAll(v);
    cout << c.total << ' ' << c.scaled(3) << endl;
    int calls = 0;
    auto nested = [&calls](int n) { calls++; return n + 1; };
    cout << nested(nested(nested(0))) << ' ' << calls << endl;
    cout << [](int a, int b) { return a * b; }(6, 7) << endl;
    auto noResult = [&]() { calls = 100; };
    noResult();
    cout << calls << endl;
    return 0;
}
