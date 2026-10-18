// Delegating constructors, static locals with arguments, using aliases,
// <sstream>, <memory>.
#include <iostream>
#include <string>
#include <sstream>
#include <memory>
#include <vector>
#include <utility>
using namespace std;

using Text = string;
using Numbers = vector<int>;

struct Point {
    int x, y;
    Point(int a, int b) : x(a), y(b) {}
    Point() : Point(0, 0) {}
    Point(int d) : Point(d, d) { x += 1; }
    Text show() const { ostringstream out; out << "(" << x << ", " << y << ")"; return out.str(); }
};

struct Resource {
    static int live;
    Text name;
    Resource(const Text &n) : name(n) { live++; cout << "open " << name << endl; }
    ~Resource() { live--; cout << "close " << name << endl; }
    void use() { cout << "using " << name << endl; }
};
int Resource::live = 0;

unique_ptr<Resource> open(const Text &name) { unique_ptr<Resource> r(new Resource(name)); return r; }

Point &origin() {
    static Point p(7);
    p.y++;
    return p;
}

int main() {
    Point a, b(3), c(1, 2);
    cout << a.show() << b.show() << c.show() << endl;
    cout << origin().show() << origin().show() << endl;

    ostringstream out;
    out << "n=" << 42 << " d=" << 2.5 << " c=" << 'z' << " b=" << true;
    Text made = out.str();
    cout << made << " [" << made.size() << "]" << endl;
    istringstream in("12 34 word 5.5 rest of it");
    int i, j; Text w; double d;
    in >> i >> j >> w >> d;
    Text rest;
    getline(in, rest);
    cout << i + j << ' ' << w << ' ' << d << " [" << rest << "]" << endl;
    stringstream lines;
    lines << "one\ntwo";
    cout << lines.str().size() << endl;
    Numbers nums;
    istringstream list("3 1 4 1 5");
    int n;
    while (list >> n) nums.push_back(n);
    cout << nums.size() << ' ' << nums[2] << endl;

    {
        unique_ptr<Resource> r = open("file");
        r->use();
        unique_ptr<Resource> s = move(r);
        cout << (r ? "r set" : "r empty") << ' ' << (s ? "s set" : "s empty") << ' ' << Resource::live << endl;
        s.reset(new Resource("other"));
        cout << Resource::live << ' ' << s->name << endl;
        shared_ptr<Resource> p = make_shared<Resource>(Text("shared"));
        shared_ptr<Resource> q = p;
        {
            shared_ptr<Resource> t(q);
            cout << p.use_count() << ' ' << (*t).name << endl;
        }
        cout << p.use_count() << ' ' << (p == q) << endl;
        q.reset();
        cout << p.use_count() << ' ' << q.use_count() << ' ' << Resource::live << endl;
        vector<shared_ptr<Resource> > keep;
        keep.push_back(p);
        keep.push_back(make_shared<Resource>(Text("vec")));
        cout << p.use_count() << ' ' << keep.size() << ' ' << Resource::live << endl;
    }
    cout << "live " << Resource::live << endl;
    return 0;
}
