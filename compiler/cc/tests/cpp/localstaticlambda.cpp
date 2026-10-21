// A generic lambda naming a static local of the function around it
// (DocumentLoader's nextIconCallbackID++ in a WTF::map lambda).
#include <stdio.h>
#include <vector>
std::vector<unsigned long> decisions(const std::vector<int>& icons)
{
    static unsigned long nextCallbackID = 1;
    std::vector<unsigned long> result;
    auto add = [&](auto& icon) { result.push_back(nextCallbackID++ + icon); };
    for (auto& icon : icons)
        add(icon);
    return result;
}
int main(int argc, char** argv)
{
    auto ids = decisions({ 7, 8 });
    auto more = decisions({ 1 });
    printf("%lu %lu %lu\n", ids[0], ids[1], more[0]);
    return argc - 1;
}
