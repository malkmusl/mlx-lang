// ::tolower handed to std::transform after <string> alone (its <cctype>).
#include <string>
#include <algorithm>
int main()
{
    std::string s = "AbC";
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);
    return s == "abc" ? 0 : 1;
}
