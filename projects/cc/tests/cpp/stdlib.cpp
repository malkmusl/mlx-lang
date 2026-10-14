// mlxlibcpp: <iostream>, <string>, the c* headers; new and delete from
// the library (nothing here defines them). Reads stdlib.stdin.
#include <iostream>
#include <string>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <cctype>

using namespace std;

struct Item {
    string name;
    int quantity;
    double price;
    Item(const string &n, int q, double p) : name(n), quantity(q), price(p) {}
    double total() const { return quantity * price; }
};

ostream &operator<<(ostream &out, const Item &item) {
    return out << item.name << " x" << item.quantity << " @ " << item.price << " = " << item.total();
}

static string repeat(const string &text, int times) {
    string result;
    for (int i = 0; i < times; i++) result += text;
    return result;
}

int main() {
    cout << "hello, " << "world" << '!' << endl;
    cout << 42 << ' ' << -7 << ' ' << 3.5 << ' ' << 1.0 / 3 << ' ' << 1e10 << ' ' << true << ' ' << 'x' << '\n';
    cout << 123456789012L << ' ' << 4000000000U << ' ' << (short)-3 << endl;
    cout.precision(3);
    cout << 3.14159265 << ' ' << 2.0 << endl;
    cout.precision(6);

    string s = "alpha";
    s += " beta";
    s.push_back('!');
    cout << s << " size " << s.size() << " find " << s.find("beta") << " none " << (s.find("gamma") == string::npos) << endl;
    string t = s.substr(6, 4);
    cout << "substr " << t << " compare " << (t < s) << (t == "beta") << ("beta" == t) << (t != s) << endl;
    cout << "concat " << s + "/" + t + '?' << endl;
    string u = t;
    u[0] = 'B';
    u.insert(0, ">>");
    u.erase(u.size() - 1);
    cout << "edited " << u << " original " << t << " empty " << string().empty() << endl;
    cout << "repeat " << repeat("ab", 3) << " to_string " << to_string(99) << to_string(2.5) << " stoi " << stoi("123") + stol("-4") << " stod " << stod("1.5") * 2 << endl;
    cout << "rfind " << s.rfind('a') << " first_of " << s.find_first_of("xyz!") << " last_not_of " << s.find_last_not_of("!") << endl;
    string parts[3] = { "one", "two", "three" };
    string joined;
    for (int i = 0; i < 3; i++) {
        if (!joined.empty()) joined += ", ";
        joined += parts[i];
    }
    cout << "joined " << joined << " length " << joined.length() << endl;
    swap(parts[0], parts[2]);
    cout << "swapped " << parts[0] << ' ' << parts[2] << endl;

    Item *items[2];
    items[0] = new Item("apple", 3, 0.5);
    items[1] = new Item("pear", 2, 0.75);
    for (int i = 0; i < 2; i++) cout << *items[i] << endl;
    for (int i = 0; i < 2; i++) delete items[i];
    string *names = new string[2];
    names[0] = "x";
    names[1] = names[0] + "y";
    cout << "names " << names[0] << names[1] << endl;
    delete[] names;

    cout << "abs " << abs(-3) << ' ' << abs(-2.5) << " sqrt " << sqrt(16.0) << " pow " << pow(2.0, 10.0) << " strlen " << strlen("four") << " upper " << (char)toupper('q') << endl;
    printf("printf still works: %s %d\n", s.c_str(), (int)s.size());

    int a, b;
    cin >> a >> b;
    string word;
    cin >> word;
    double d;
    cin >> d;
    cout << "read " << a + b << ' ' << word << ' ' << d << endl;
    cin.ignore(100, '\n');
    string line;
    getline(cin, line);
    cout << "line [" << line << "]" << endl;
    int count = 0;
    while (true) {
        getline(cin, line);
        if (cin.fail()) break;
        count++;
        cout << count << ": " << line << endl;
    }
    cout << "lines " << count << " eof " << cin.eof() << endl;
    cerr << "to stderr" << endl;
    return 0;
}
