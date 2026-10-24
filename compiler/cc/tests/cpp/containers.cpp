// mlxlibcpp's templates: <vector>, <utility>, <algorithm>.
#include <vector>
#include <utility>
#include <algorithm>
#include <numeric>
#include <string>
#include <iostream>
using namespace std;

struct Tracked {
    static int live;
    int id;
    Tracked() : id(0) { live++; }
    Tracked(int i) : id(i) { live++; }
    Tracked(const Tracked &o) : id(o.id) { live++; }
    ~Tracked() { live--; }
};
int Tracked::live = 0;

static void show(const vector<int> &v) {
    cout << "[";
    for (size_t i = 0; i < v.size(); i++) cout << (i ? " " : "") << v[i];
    cout << "]";
}

int main() {
    vector<int> v;
    for (int i = 0; i < 10; i++) v.push_back((i * 7) % 10);
    show(v);
    cout << " size " << v.size() << " cap " << v.capacity() << " front " << v.front() << " back " << v.back() << endl;
    sort(v.begin(), v.end());
    show(v);
    cout << " found " << (find(v.begin(), v.end(), 8) - v.begin()) << " missing " << (find(v.begin(), v.end(), 11) == v.end()) << " count " << count(v.begin(), v.end(), 3) << endl;
    reverse(v.begin(), v.end());
    v.erase(v.begin() + 2);
    v.insert(v.begin(), 42);
    v.pop_back();
    show(v);
    cout << " sum " << accumulate(v.begin(), v.end(), 0) << " min " << min(3, 4) << " max " << max(string("a"), string("b")) << endl;
    vector<int> w = v;
    w[0] = 1;
    cout << "equal " << (v == w) << (v != w) << ' ' << equal(v.begin() + 1, v.end(), w.begin() + 1) << endl;
    vector<int> sorted(v);
    sort(sorted.begin(), sorted.end());
    vector<int>::iterator last = unique(sorted.begin(), sorted.end());
    sorted.erase(last, sorted.end());
    show(sorted);
    cout << endl;

    vector<string> names;
    names.push_back("delta");
    names.push_back("alpha");
    names.push_back("charlie");
    names.push_back("bravo");
    sort(names.begin(), names.end());
    for (vector<string>::iterator it = names.begin(); it != names.end(); ++it) cout << *it << ' ';
    cout << names.size() << endl;
    names.resize(2);
    names.push_back(names[0] + "!");
    cout << names[2] << ' ' << names.size() << endl;

    {
        vector<Tracked> tracked;
        for (int i = 1; i <= 20; i++) tracked.push_back(Tracked(i));
        cout << "live " << Tracked::live << " size " << tracked.size() << ' ' << tracked[19].id << endl;
        tracked.erase(tracked.begin());
        tracked.resize(5);
        cout << "live " << Tracked::live << ' ' << tracked[0].id << ' ' << tracked.back().id << endl;
        vector<Tracked> more(3);
        more.assign(4, Tracked(9));
        cout << "live " << Tracked::live << ' ' << more[3].id << endl;
    }
    cout << "live " << Tracked::live << endl;

    pair<string, int> p = make_pair(string("age"), 30);
    pair<int, int> q(1, 2), r(1, 3);
    cout << p.first << ' ' << p.second << ' ' << (q < r) << (q == r) << (q != r) << endl;
    vector<pair<string, int> > table;
    table.push_back(p);
    table.push_back(make_pair(string("height"), 180));
    swap(table[0], table[1]);
    cout << table[0].first << ' ' << table[1].second << endl;
    vector<vector<int> > grid(3, vector<int>(2, 7));
    grid[1][1] = 0;
    cout << grid.size() << ' ' << grid[0][0] << grid[1][1] << grid[2][1] << endl;
    int raw[5] = { 5, 3, 1, 4, 2 };
    sort(raw, raw + 5);
    fill(raw + 3, raw + 5, 0);
    for (int i = 0; i < 5; i++) cout << raw[i];
    cout << endl;
    return 0;
}
