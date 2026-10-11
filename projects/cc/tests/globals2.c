/* The second file of globals.c's program. It includes what the first
   file includes: each file starts with its own macros, so the headers'
   guards do not hide them here. */
#include <stdio.h>

void report(const char *what) { printf("report: %s\n", what); }
int shared = 41;
const char *message = "from the other file";
static int hidden = 1000;

int counter(void) {
    static int count;
    return ++count + hidden;
}

int helper(int x) {
    extern int shared;
    return x * 100 + shared;
}
