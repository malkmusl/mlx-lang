// std::stringstream read with >> (Skia's SkSL::stod: a number of a
// shader's source read from a stringstream over its text), and written
// then read.
#include <locale>
#include <sstream>
#include <string>
#include <cstdio>

int main(int argc, char **argv) {
    const char *texts[] = { "0.", "1.5", "2", "1e3", "x" };
    for (const char *text : texts) {
        float value = -1;
        std::stringstream buffer(std::string { text });
        buffer.imbue(std::locale::classic());
        buffer >> value;
        printf("%s %g %d\n", text, value, buffer.fail() ? 1 : 0);
    }
    std::stringstream both;
    both << 12 << ' ' << "word";
    int number = 0;
    std::string word;
    both >> number >> word;
    printf("%d %s %d\n", number, word.c_str(), both.fail() ? 1 : 0);
    return argc;
}
