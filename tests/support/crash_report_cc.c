/* tools/check_crash_report.sh: a C program mlx-cc compiles that crashes on
   purpose two calls deep (in a static function); its report must name
   each function and where it was defined. */
#include <stdio.h>

struct thing { int *where; };

static int inner(struct thing *t) { return *t->where + 1; }

int middle(struct thing *t) { return inner(t) * 2; }

int main(void) {
    struct thing t = { 0 };
    printf("about to crash\n");
    fflush(stdout);
    return middle(&t);
}
