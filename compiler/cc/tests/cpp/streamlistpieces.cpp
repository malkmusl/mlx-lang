// Pieces of mlxlibcpp the conformance suite uses: std::list with its
// allocator named, getline on a stringstream with a delimiter, a stream's
// state set, read and cleared.
#include <stdio.h>
#include <list>
#include <memory>
#include <sstream>
#include <string>

int main()
{
    std::list<int, std::allocator<int>> numbers;
    numbers.push_back(2);
    numbers.push_front(1);
    numbers.push_back(3);
    int sum = 0;
    for (std::list<int, std::allocator<int>>::const_iterator i = numbers.begin(); i != numbers.end(); ++i)
        sum += *i;
    printf("%zu %d\n", numbers.size(), sum);

    std::stringstream words("alpha,beta,,gamma");
    std::string word;
    while (std::getline(words, word, ','))
        printf("[%s]", word.c_str());
    printf("\n");

    std::ostringstream out;
    out << "x";
    printf("%d %d\n", out.good() ? 1 : 0, out.fail() ? 1 : 0);
    out.setstate(std::ios_base::failbit);
    printf("%d %d\n", out.good() ? 1 : 0, out.fail() ? 1 : 0);
    out.clear();
    printf("%d %d\n", out.good() ? 1 : 0, (out.rdstate() == std::ios_base::goodbit) ? 1 : 0);
    return 0;
}
