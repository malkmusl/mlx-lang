// The streams' ios_base names through the stream classes
// (`std::ofstream::binary`, ANGLE's pipeline cache graph dump), and an
// ofstream made by a temporary (`std::ofstream out = std::ofstream(...)`).
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdio.h>
#include <string>

int main() {
    std::string path = "streamopenmode.out";
    std::ofstream out = std::ofstream(path, std::ofstream::binary);
    if (!out.is_open()) return 1;
    out << "line " << 42 << "\n";
    out.close();
    std::ifstream in(path, std::ifstream::in);
    std::string word;
    int number = 0;
    in >> word >> number;
    std::istringstream text("7");
    int seven = 0;
    text >> seven;
    printf("%s %d %d %d %d\n", word.c_str(), number, seven, std::ostream::binary == std::ios_base::binary, std::istream::out == std::ios::out);
    remove(path.c_str());
    return 0;
}
