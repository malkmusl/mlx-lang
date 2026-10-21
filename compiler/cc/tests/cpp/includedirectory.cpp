// #include <numeric> with a directory "numeric" in a -I directory
// (includedirectory.flags): the header, not the directory.
#include <cstdio>
#include <numeric>

int main(int argc, char** argv)
{
    std::printf("%d %d %g\n", std::midpoint(argc, 9), std::gcd(12, 18), std::midpoint(1.0, 2.0));
    return 0;
}
