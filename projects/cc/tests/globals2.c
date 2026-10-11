/* The second file of globals.c's program. */
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
