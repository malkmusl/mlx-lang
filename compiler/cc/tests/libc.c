/* The C library from compiled code: memory, strings, conversion,
   files, errno, the environment. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

static int byLength(const void *a, const void *b) {
    const char *x = *(char *const *)a, *y = *(char *const *)b;
    int difference = (int)strlen(x) - (int)strlen(y);
    return difference ? difference : strcmp(x, y);
}

int main(int argc, char **argv) {
    printf("argc %d %s\n", argc, argc > 1 ? argv[1] : "-");
    char *copy = strdup("Hello, C");
    for (char *p = copy; *p; p++) *p = toupper((unsigned char)*p);
    printf("%s\n", copy);
    free(copy);
    printf("%d %ld %lu\n", atoi("-123"), strtol("ff", NULL, 16), strtoul("777", NULL, 8));
    char *end;
    double d = strtod("2.5e3xyz", &end);
    printf("%g %s\n", d, end);
    char *words[] = { "banana", "fig", "cherry", "kiwi" };
    qsort(words, 4, sizeof(char *), byLength);
    printf("%s %s %s %s\n", words[0], words[1], words[2], words[3]);
    printf("%s\n", strstr("needle in haystack", "hay"));
    const char *letters = "abcdef";
    printf("%d\n", (int)(strchr(letters, 'd') - letters));
    char path[] = "/tmp/mlxcc-libc-test.txt";
    FILE *f = fopen(path, "w");
    fprintf(f, "line %d\n", 1);
    fputs("line 2\n", f);
    fclose(f);
    f = fopen(path, "r");
    char line[64];
    int lines = 0;
    while (fgets(line, sizeof line, f)) lines++;
    fclose(f);
    unlink(path);
    printf("lines %d\n", lines);
    errno = 0;
    int fd = open("/nonexistent/file", O_RDONLY);
    printf("%d %d\n", fd, errno == ENOENT);
    setenv("MLXCC_TEST", "value", 1);
    printf("%s\n", getenv("MLXCC_TEST"));
    div_t q = div(17, 5);
    printf("%d %d\n", q.quot, q.rem);
    printf("%d %d %d\n", abs(-4), isdigit('7') != 0, isspace('x') != 0);
    char num[32];
    snprintf(num, sizeof num, "%08.3f|%-6s|%+d", 3.14159, "ab", 5);
    printf("%s\n", num);
    write(1, "direct\n", 7);
    fflush(stdout);
    return 0;
}
