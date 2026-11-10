// A range-for over an array of arrays: `auto &row` is the row (an array to
// loop over in turn), `auto row` a pointer to its first element.
#include <stdio.h>
struct Image { int id; };
int main() {
    Image images[2][3];
    int next = 0;
    for (auto &line : images) {
        for (auto &image : line) { image.id = next++; }
    }
    Image (&second)[3] = images[1];
    int sum = 0;
    for (const Image &image : second) { sum += image.id; }
    for (auto row : images) { sum += row[0].id * 100; }
    printf("%d %d\n", images[1][2].id, sum);
    return 0;
}
