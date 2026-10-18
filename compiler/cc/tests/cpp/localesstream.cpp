// <locale> (std::isspace(char, const locale &)) included before
// <sstream>: the stream's own reading still splits at white space
// (Skia's SkSLString.cpp).
#include <locale>
#include <sstream>
#include <string>
#include <cstdio>

int main(int argc, char **argv) {
    std::istringstream in("alpha  beta\tgamma");
    std::string a, b, c;
    in >> a >> b >> c;
    std::locale classic = std::locale::classic();
    printf("%s|%s|%s %d %d\n", a.c_str(), b.c_str(), c.c_str(), std::isspace(' ', classic), std::isdigit('7', classic));
    return argc;
}
