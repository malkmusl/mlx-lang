// Conversion operators, default template arguments, the range-based
// for, auto, and members of a class template's instance read only when
// called (vector<Tracked> without resize's default constructor).
#include <iostream>
#include <string>
#include <vector>
using namespace std;

struct Maybe {
    int value; bool ok;
    Maybe() : value(0), ok(false) {}
    Maybe(int v) : value(v), ok(true) {}
    operator bool() const { return ok; }
    operator int() const { return value; }
};
struct Name { string text; Name(const char *t) : text(t) {} operator const char *() const { return text.c_str(); } };
template <typename T, int N = 3, typename U = T> struct Rep { T items[N]; U extra; int size() const { return N; } };
int takes(int x) { return x * 2; }
struct Tracked {
    int id;
    Tracked(int i) : id(i) { cout << "+" << i; }
    Tracked(const Tracked &o) : id(o.id) { cout << "c" << id; }
    ~Tracked() { cout << "-" << id; }
};
vector<int> squares(int n) { vector<int> v; for (int i = 1; i <= n; i++) v.push_back(i * i); return v; }

int main() {
    Maybe a(5), b;
    if (a) cout << "a ok ";
    if (!b) cout << "b not ";
    cout << (a && b) << (a || b) << ' ' << (a ? "yes" : "no") << ' ' << takes(a) << ' ' << (int)a + 1 << endl;
    bool flag = a;
    int n = a;
    Name nm("hello");
    cout << flag << n << ' ' << (const char *)nm << ' ' << Rep<int>().size() << ' ' << Rep<double, 5>().size() << ' ' << sizeof(Rep<int, 2, double>) << endl;
    while (a) { a.ok = false; cout << "once "; }
    cout << endl;

    int raw[4] = { 1, 2, 3, 4 };
    int sum = 0;
    for (int x : raw) sum += x;
    for (auto &x : raw) x *= 10;
    cout << sum << ' ' << raw[3] << endl;
    vector<string> words;
    words.push_back("a"); words.push_back("bb"); words.push_back("ccc");
    for (const auto &w : words) cout << w << '/';
    for (string w : words) { w += "!"; cout << w; }
    cout << endl;
    for (auto v : squares(3)) { if (v == 4) continue; cout << v << ' '; if (v > 5) break; }
    cout << endl;
    vector<Tracked> ts;
    ts.reserve(2);
    ts.push_back(Tracked(1));
    ts.push_back(Tracked(2));
    cout << endl;
    for (Tracked t : ts) cout << " [" << t.id << "]";
    cout << endl;
    for (auto it = words.begin(); it != words.end(); ++it) cout << it->size();
    auto *p = &raw[0];
    auto count = words.size();
    cout << ' ' << *p << ' ' << count << endl;
    return 0;
}
