// mlxlibcpp's <map> and <set>: counting words, lookups, iteration in
// key order, erasure, nested containers, insert's result.
#include <map>
#include <set>
#include <vector>
#include <string>
#include <iostream>
using namespace std;

int main() {
    const char *text[] = { "the", "cat", "sat", "on", "the", "mat", "the", "end" };
    map<string, int> counts;
    for (const char *word : text) counts[word]++;
    for (map<string, int>::iterator it = counts.begin(); it != counts.end(); ++it) cout << it->first << '=' << it->second << ' ';
    cout << "| size " << counts.size() << " the " << counts["the"] << " dog " << counts.count("dog") << endl;
    counts.erase("the");
    cout << "erased " << counts.count("the") << " size " << counts.size() << " find " << (counts.find("cat") != counts.end()) << (counts.find("the") == counts.end()) << endl;
    pair<map<string, int>::iterator, bool> added = counts.insert(make_pair(string("cat"), 99));
    pair<map<string, int>::iterator, bool> fresh = counts.insert(make_pair(string("zebra"), 1));
    cout << added.second << added.first->second << ' ' << fresh.second << fresh.first->first << endl;

    map<int, string> names;
    names[3] = "three";
    names[1] = "one";
    names[2] = "two";
    for (auto &entry : names) cout << entry.first << ':' << entry.second << ' ';
    cout << names.lower_bound(2)->second << ' ' << names.upper_bound(2)->second << endl;

    set<string> seen;
    for (const char *word : text) seen.insert(word);
    for (const string &w : seen) cout << w << ' ';
    cout << "| " << seen.size() << ' ' << seen.count("cat") << seen.count("dog") << ' ' << seen.insert("cat").second << seen.insert("dog").second << endl;
    seen.erase("the");
    set<int> numbers;
    for (int i = 10; i > 0; i--) numbers.insert(i % 4);
    for (int n : numbers) cout << n;
    cout << ' ' << seen.size() << ' ' << *numbers.begin() << ' ' << *numbers.find(3) << endl;

    map<string, vector<int> > groups;
    groups["even"].push_back(2);
    groups["odd"].push_back(1);
    groups["even"].push_back(4);
    groups["odd"].push_back(3);
    for (auto &g : groups) {
        cout << g.first << ":";
        for (int v : g.second) cout << ' ' << v;
        cout << ' ';
    }
    map<string, vector<int> > copy = groups;
    copy["even"].push_back(6);
    cout << groups["even"].size() << copy["even"].size() << ' ' << (groups == copy) << endl;
    return 0;
}
