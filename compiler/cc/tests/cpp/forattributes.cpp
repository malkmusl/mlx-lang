// Attributes on a for statement's declaration, in a range for and an
// ordinary one (Skia's SubRunContainer: `for ([[maybe_unused]] auto &x :
// runs)`).
#include <cstdio>
#include <vector>

int main(int argc, char **argv) {
    std::vector<int> runs = {3, 4, 5};
    int count = 0;
    for ([[maybe_unused]] auto &run : runs) {
        count += 1;
    }
    int sum = 0;
    for ([[maybe_unused]] int i = 0; i < 4; i++) sum += i;
    printf("%d %d\n", count, sum);
    return argc;
}
