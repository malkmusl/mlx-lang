// mlxlibc against glibc from C: tools/check_mlxlibc.sh builds this program
// twice with gcc, once linked against libmlxc.so (whose symbols the dynamic
// linker finds before glibc's) and once against glibc alone, runs both and
// compares what they print. Everything printed is deterministic (no
// addresses, times or ids). The work directory for files is argv[1];
// argv[2] names the library the run expects under it (mlx or glibc).
//
//   mlxlibc_check WORKDIR mlx|glibc [PLUGIN.so]
//
// With PLUGIN.so (an Mlx --plugin shared object, tests/support/plugin_library.mlx
// built by the check script) the dlfcn functions are exercised on it.
#define _GNU_SOURCE
// Truncating copies and formats are what some of the calls below check.
#pragma GCC diagnostic ignored "-Wstringop-truncation"
#pragma GCC diagnostic ignored "-Wformat-truncation"
#include <ctype.h>
#include <dlfcn.h>
#include <errno.h>
#include <link.h>
#include <fcntl.h>
#include <inttypes.h>
#include <libgen.h>
#include <limits.h>
#include <malloc.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

// libmlxc exports this, glibc does not: which library the run is on.
extern const char *mlxlibc_version(void) __attribute__((weak));

static void section(const char *name) { printf("== %s\n", name); }

static void check_printf(void) {
    section("printf");
    printf("[%d|%5d|%-5d|%05d|%+d|% d|%.3d|%x|%X|%#x|%o|%#o|%u|%c|%hhd|%hd]\n", 42, 42, 42, 42, 42, 42, 42, 255, 255, 255, 8, 8, 42u, 'A', 300, 70000);
    printf("[%ld|%lld|%lu|%zu|%jd|%td|%" PRId64 "|%" PRIu32 "]\n", -1L, LLONG_MIN, ULONG_MAX, (size_t)1 << 40, (intmax_t)-5, (ptrdiff_t)7, (int64_t)INT64_MAX, (uint32_t)4000000000u);
    printf("[%s|%10s|%-10s|%.3s|%.0s|%s]\n", "text", "right", "left", "truncate", "gone", "");
    printf("[%p|%%|%5.1f%%]\n", (void *)0, 99.5);
    double values[] = {0.0, -0.0, 1.0, 0.5, 3.14159, -2.71828, 1e10, 1e-10, 123456789.125, 0.1, 2.0 / 3.0, 1e300, 5e-324, 999999.5, 1e21, 0.000012345, 100.0, 0.0625};
    for (size_t i = 0; i < sizeof values / sizeof values[0]; i++) {
        double v = values[i];
        printf("%f|%.0f|%.2f|%10.3f|%e|%.3E|%g|%.10g|%G|%#g|%a|%.3a|%.20f\n", v, v, v, v, v, v, v, v, v, v, v, v, v);
    }
    printf("[%f|%e|%g|%F|%a]\n", __builtin_inf(), -__builtin_inf(), __builtin_nan(""), __builtin_inf(), __builtin_inf());
    printf("[%*d|%-*d|%.*f|%*.*f|%*d]\n", 6, 42, 6, 42, 2, 3.14159, 10, 3, 2.5, -6, 42);
    char buffer[16];
    int n = snprintf(buffer, sizeof buffer, "%s and %d and %s", "hello", 12345, "more");
    printf("snprintf %d \"%s\" %d\n", n, buffer, snprintf(NULL, 0, "%d", 12345));
    char *made = NULL;
    n = asprintf(&made, "%s=%d", "key", 99);
    printf("asprintf %d \"%s\"\n", n, made);
    free(made);
    int count = -1;
    snprintf(buffer, sizeof buffer, "abc%n", &count);
    errno = ENOENT;
    printf("%%n %d %%m %m\n", count);
    int a = 0, b = 0, c = 0;
    char word[16];
    double d = 0;
    int matched = sscanf("12 34 word 2.5", "%d %d %15s %lf", &a, &b, word, &d);
    printf("sscanf %d %d %d %s %.1f\n", matched, a, b, word, d);
    matched = sscanf("0x1f 077 99", "%i %i %i", &a, &b, &c);
    printf("sscanf %d %d %d %d\n", matched, a, b, c);
    matched = sscanf("key=value", "%15[^=]=%15s", word, buffer);
    printf("sscanf %d %s %s\n", matched, word, buffer);
}

static int compare_ints(const void *left, const void *right) {
    int a = *(const int *)left, b = *(const int *)right;
    return (a > b) - (a < b);
}

static int compare_strings(const void *left, const void *right) {
    return strcmp(*(const char *const *)left, *(const char *const *)right);
}

static void check_strings(void) {
    section("strings");
    char buffer[64];
    printf("%zu %zu %d %d %d %d\n", strlen(""), strlen("hello world"), strcmp("abc", "abd"), strncmp("abcdef", "abcxyz", 3), strcasecmp("HeLLo", "hello"), strncasecmp("HeLLo", "help", 3));
    strcpy(buffer, "hello");
    strcat(buffer, ", ");
    strncat(buffer, "world!!!", 5);
    printf("%s|%s|%s|%s|%d\n", buffer, strchr(buffer, 'w'), strrchr(buffer, 'l'), strstr(buffer, "lo,"), strchr(buffer, 'z') == NULL);
    printf("%zu %zu %s %s\n", strspn("123abc", "0123456789"), strcspn("abc;def", ";"), strpbrk("hello world", "ow"), strcasestr("Hello World", "o w"));
    char text[] = "a,b,,c;d";
    char *save = NULL;
    for (char *piece = strtok_r(text, ",;", &save); piece; piece = strtok_r(NULL, ",;", &save)) printf("<%s>", piece);
    char *copy = strdup("duplicate");
    char *part = strndup("duplicate", 3);
    printf(" %s %s\n", copy, part);
    free(copy);
    free(part);
    unsigned char bytes[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    memmove(bytes + 2, bytes, 4);
    memset(bytes + 6, 0xff, 2);
    for (int i = 0; i < 8; i++) printf("%02x", bytes[i]);
    const char hello[] = "hello";
    printf(" %d %d %d %d\n", memcmp("abc", "abd", 3), memcmp("abc", "abc", 3), (int)((const char *)memchr(hello, 'l', 5) - hello), (int)((const char *)memrchr(hello, 'l', 5) - hello));
    char err[64];
    printf("%s|%s|%s|%s\n", strerror(ENOENT), strerror(EACCES), strerror(0), strerror_r(EINVAL, err, sizeof err));
    printf("%s|%s\n", strsignal(SIGSEGV), strsignal(SIGINT));
    printf("%d %d %d\n", ffs(0), ffs(8), ffsl(1L << 40));
    char path1[] = "/usr/lib/libmlxc.so.1", path2[] = "/usr/lib/libmlxc.so.1", path3[] = "noslash";
    printf("%s %s %s\n", basename(path1), dirname(path2), basename(path3));
    snprintf(buffer, sizeof buffer, "%s", "0123456789");
    printf("%s %s %zu\n", stpcpy(buffer, "ab") == buffer + 2 ? buffer : "?", stpncpy(buffer, "xyz", 2) == buffer + 2 ? buffer : "?", strnlen("hello", 3));
}

static void check_ctype(void) {
    section("ctype");
    for (int c = -1; c < 128; c++) {
        int bits = (isalpha(c) != 0) | (isdigit(c) != 0) << 1 | (isspace(c) != 0) << 2 | (isupper(c) != 0) << 3 | (islower(c) != 0) << 4 | (ispunct(c) != 0) << 5 | (isprint(c) != 0) << 6 | (iscntrl(c) != 0) << 7 | (isxdigit(c) != 0) << 8 | (isalnum(c) != 0) << 9 | (isgraph(c) != 0) << 10 | (isblank(c) != 0) << 11;
        printf("%03x%c", bits, c % 16 == 15 ? '\n' : ' ');
    }
    printf("\n%c%c %d %d\n", toupper('a'), tolower('Q'), toupper('1'), tolower(EOF));
}

static void check_stdlib(void) {
    section("stdlib");
    char *end;
    errno = 0;
    long l = strtol("  -1234xyz", &end, 10);
    printf("%ld '%s' %d\n", l, end, errno);
    printf("%ld %ld %lu %lld\n", strtol("0x1F", NULL, 16), strtol("0x1F", NULL, 0), strtoul("777", NULL, 8), strtoll("-9223372036854775808", NULL, 10));
    errno = 0;
    unsigned long big = strtoul("99999999999999999999", &end, 10);
    printf("%lu %d\n", big, errno == ERANGE);
    errno = 0;
    l = strtol("zzz", &end, 36);
    printf("%ld %d\n", l, errno);
    printf("%.17g %.17g %.17g %g %g %.17g\n", strtod("3.14159", NULL), strtod("1e-320", NULL), strtod("0x1.8p1", NULL), strtod("inf", NULL), strtod("  -0.0", NULL), strtod("1.7976931348623157e308", NULL));
    printf("%d %ld %lld %.2f\n", atoi("  42abc"), atol("-7"), atoll("123456789012"), atof("2.5e2"));
    int numbers[] = {5, 3, 9, 1, 7, 3, 8, 2};
    qsort(numbers, 8, sizeof numbers[0], compare_ints);
    for (int i = 0; i < 8; i++) printf("%d ", numbers[i]);
    int key = 7;
    int *found = bsearch(&key, numbers, 8, sizeof numbers[0], compare_ints);
    key = 4;
    printf("| %d %d\n", found ? *found : -1, bsearch(&key, numbers, 8, sizeof numbers[0], compare_ints) == NULL);
    const char *names[] = {"pear", "apple", "fig", "banana"};
    qsort(names, 4, sizeof names[0], compare_strings);
    printf("%s %s %s %s\n", names[0], names[1], names[2], names[3]);
    div_t q = div(7, -2);
    ldiv_t lq = ldiv(-7, 2);
    printf("%d %d %ld %ld %d %ld %lld\n", q.quot, q.rem, lq.quot, lq.rem, abs(-5), labs(-6L), llabs(-7LL));
    srand(1);
    int r1 = rand(), r2 = rand(), r3 = rand();
    srand(42);
    int r4 = rand(), r5 = rand();
    printf("%d %d %d %d %d\n", r1, r2, r3, r4, r5);
    printf("%s %s\n", getenv("MLXLIBC_CHECK"), getenv("MLXLIBC_MISSING") ? "set" : "(unset)");
    setenv("MLXLIBC_SET", "value", 1);
    const char *first = getenv("MLXLIBC_SET");
    setenv("MLXLIBC_SET", "other", 0);
    const char *second = getenv("MLXLIBC_SET");
    setenv("MLXLIBC_SET", "third", 1);
    const char *third = getenv("MLXLIBC_SET");
    unsetenv("MLXLIBC_SET");
    printf("%s %s %s %s\n", first, second, third, getenv("MLXLIBC_SET") ? "still" : "gone");
    int status = system("exit 3");
    printf("system %d %d\n", WIFEXITED(status), WEXITSTATUS(status));
}

static void check_malloc(void) {
    section("malloc");
    size_t sizes[] = {1, 7, 16, 100, 4096, 65536, 1 << 20};
    for (size_t i = 0; i < sizeof sizes / sizeof sizes[0]; i++) {
        unsigned char *block = malloc(sizes[i]);
        memset(block, (int)i + 1, sizes[i]);
        block = realloc(block, sizes[i] * 2 + 1);
        int kept = block[0] == i + 1 && block[sizes[i] - 1] == i + 1;
        int usable = malloc_usable_size(block) >= sizes[i] * 2 + 1;
        block = realloc(block, 3);
        printf("%zu:%d:%d:%d ", sizes[i], kept, usable, block[0] == i + 1);
        free(block);
    }
    unsigned char *zeros = calloc(100, 10);
    int clean = 1;
    for (int i = 0; i < 1000; i++) clean &= zeros[i] == 0;
    free(zeros);
    void *aligned = NULL;
    int r = posix_memalign(&aligned, 4096, 100);
    void *other = aligned_alloc(64, 128);
    printf("| %d %d %d %d\n", clean, r, ((uintptr_t)aligned & 4095) == 0, ((uintptr_t)other & 63) == 0);
    free(aligned);
    free(other);
    free(NULL);
}

static void check_stdio(const char *work) {
    section("stdio");
    char path[PATH_MAX];
    snprintf(path, sizeof path, "%s/check.txt", work);
    FILE *f = fopen(path, "w");
    int ok = f != NULL;
    int r1 = fputs("line one\n", f);
    int r2 = fprintf(f, "%s %d %.2f\n", "line", 2, 2.5);
    size_t r3 = fwrite("line three\n", 1, 11, f);
    int r4 = fputc('A', f);
    long position = ftell(f);
    printf("%d %d %d %zu %d %ld %d\n", ok, r1, r2, r3, r4, position, fclose(f));
    f = fopen(path, "r");
    char line[64];
    printf("%s", fgets(line, sizeof line, f) ? line : "(none)\n");
    int c = fgetc(f);
    int back = ungetc('L', f);
    const char *five = fgets(line, 6, f);
    printf("%c %c %s| %ld %d ", c, back, five ? five : "?", ftell(f), fseek(f, -5, SEEK_CUR));
    size_t got = fread(line, 1, 4, f);
    line[got] = 0;
    printf("%zu %s ", got, line);
    fseek(f, 0, SEEK_END);
    long end = ftell(f);
    int last = fgetc(f);
    int at_end = feof(f);
    rewind(f);
    printf("%ld %d %d %d ", end, last, at_end, feof(f));
    char *dynamic = NULL;
    size_t capacity = 0;
    ssize_t length = getline(&dynamic, &capacity, f);
    printf("%zd %s", length, dynamic);
    free(dynamic);
    char w1[16], w2[16];
    int matched = fscanf(f, "%15s %15s", w1, w2);
    printf("%d %s %s\n", matched, w1, w2);
    fclose(f);
    f = fopen(path, "a");
    fputs("appended\n", f);
    fclose(f);
    struct stat info;
    stat(path, &info);
    printf("size %lld\n", (long long)info.st_size);
    FILE *missing = fopen("/no/such/file", "r");
    printf("%d %d\n", missing == NULL, errno == ENOENT);
    f = fopen(path, "w");
    setvbuf(f, NULL, _IONBF, 0);
    fputs("unbuffered", f);
    stat(path, &info);
    printf("%lld %d\n", (long long)info.st_size, fclose(f));
    FILE *t = tmpfile();
    fputs("tmp", t);
    rewind(t);
    got = fread(line, 1, 8, t);
    line[got] = 0;
    printf("%zu %s %d\n", got, line, fclose(t));
    int removed = remove(path);
    printf("%d %d\n", removed, access(path, F_OK));
    fprintf(stderr, "to stderr %d\n", 1);
    errno = EACCES;
    perror("perror");
}

static void check_time(void) {
    section("time");
    time_t stamps[] = {0, 1, 86399, 951782400, 1234567890, 1700000000, 2147483647, 4102444800, -1, 253402300799};
    char text[128];
    for (size_t i = 0; i < sizeof stamps / sizeof stamps[0]; i++) {
        struct tm parts;
        gmtime_r(&stamps[i], &parts);
        strftime(text, sizeof text, "%Y-%m-%d %H:%M:%S %a %b %j %U %W %V %G %u %w %e %p %I %y %C %Z %%", &parts);
        printf("%lld: %s | %s", (long long)stamps[i], text, asctime(&parts));
        printf("timegm %lld isdst %d yday %d\n", (long long)timegm(&parts), parts.tm_isdst, parts.tm_yday);
    }
    struct tm leap = {.tm_year = 124, .tm_mon = 1, .tm_mday = 29, .tm_hour = 12};
    printf("%lld %.1f\n", (long long)timegm(&leap), difftime(100, 40));
    struct timespec now;
    int r = clock_gettime(CLOCK_REALTIME, &now);
    printf("%d %d %d\n", r, now.tv_sec > 1700000000, time(NULL) > 1700000000);
}

static pthread_mutex_t counter_lock = PTHREAD_MUTEX_INITIALIZER;
static long counter = 0;
// Thread-local storage of the program itself: each thread's own copy,
// starting from the initialization image.
static __thread long per_thread = 5;
static __thread char per_thread_text[32];

static void *count_up(void *argument) {
    long rounds = (long)argument;
    for (long i = 0; i < rounds; i++) {
        pthread_mutex_lock(&counter_lock);
        counter++;
        pthread_mutex_unlock(&counter_lock);
        per_thread++;
    }
    per_thread_text[0] = 'a' + (char)(rounds % 26);
    return (void *)(per_thread * 1000 + per_thread_text[0]);
}

static pthread_mutex_t flag_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t flag_cond = PTHREAD_COND_INITIALIZER;
static int flag = 0, seen = 0;

static void *wait_for_flag(void *argument) {
    (void)argument;
    pthread_mutex_lock(&flag_lock);
    while (flag == 0) pthread_cond_wait(&flag_cond, &flag_lock);
    seen = flag;
    pthread_mutex_unlock(&flag_lock);
    return NULL;
}

static pthread_once_t once = PTHREAD_ONCE_INIT;
static int once_count = 0;
static void run_once(void) { once_count++; }

static pthread_key_t key;
static int destroyed = 0;
static void destroy_value(void *value) { destroyed += (int)(intptr_t)value; }
static void *use_key(void *argument) {
    pthread_setspecific(key, argument);
    return pthread_getspecific(key);
}

static pthread_barrier_t barrier;
static void *at_barrier(void *argument) {
    (void)argument;
    return (void *)(intptr_t)(pthread_barrier_wait(&barrier) == PTHREAD_BARRIER_SERIAL_THREAD);
}

static void check_pthread(void) {
    section("pthread");
    pthread_t threads[4];
    for (long i = 0; i < 4; i++) pthread_create(&threads[i], NULL, count_up, (void *)(5000L + i));
    long total = 0;
    for (int i = 0; i < 4; i++) {
        void *result;
        pthread_join(threads[i], &result);
        total += (long)result;
    }
    per_thread += 2;
    printf("%ld %ld %ld %d\n", counter, total, per_thread, per_thread_text[0]);
    pthread_t waiter;
    pthread_create(&waiter, NULL, wait_for_flag, NULL);
    usleep(20000);
    pthread_mutex_lock(&flag_lock);
    flag = 9;
    pthread_cond_signal(&flag_cond);
    pthread_mutex_unlock(&flag_lock);
    pthread_join(waiter, NULL);
    pthread_once(&once, run_once);
    pthread_once(&once, run_once);
    printf("%d %d\n", seen, once_count);
    pthread_key_create(&key, destroy_value);
    pthread_setspecific(key, (void *)100);
    void *result;
    pthread_create(&waiter, NULL, use_key, (void *)7);
    pthread_join(waiter, &result);
    printf("%d %d %d\n", (int)(intptr_t)result, destroyed, (int)(intptr_t)pthread_getspecific(key));
    pthread_barrier_init(&barrier, NULL, 3);
    pthread_t two[2];
    for (int i = 0; i < 2; i++) pthread_create(&two[i], NULL, at_barrier, NULL);
    int serial = pthread_barrier_wait(&barrier) == PTHREAD_BARRIER_SERIAL_THREAD;
    for (int i = 0; i < 2; i++) {
        pthread_join(two[i], &result);
        serial += (int)(intptr_t)result;
    }
    pthread_barrier_destroy(&barrier);
    printf("serial %d\n", serial);
    pthread_mutexattr_t attributes;
    pthread_mutexattr_init(&attributes);
    pthread_mutexattr_settype(&attributes, PTHREAD_MUTEX_ERRORCHECK);
    pthread_mutex_t checked;
    pthread_mutex_init(&checked, &attributes);
    int m1 = pthread_mutex_lock(&checked);
    int m2 = pthread_mutex_lock(&checked) == EDEADLK;
    int m3 = pthread_mutex_unlock(&checked);
    int m4 = pthread_mutex_unlock(&checked) == EPERM;
    pthread_mutex_destroy(&checked);
    pthread_rwlock_t rw = PTHREAD_RWLOCK_INITIALIZER;
    int r1 = pthread_rwlock_rdlock(&rw);
    int r2 = pthread_rwlock_tryrdlock(&rw);
    int r3 = pthread_rwlock_trywrlock(&rw) == EBUSY;
    pthread_rwlock_unlock(&rw);
    pthread_rwlock_unlock(&rw);
    int r4 = pthread_rwlock_trywrlock(&rw);
    pthread_rwlock_unlock(&rw);
    printf("%d %d %d %d | %d %d %d %d | %d\n", m1, m2, m3, m4, r1, r2, r3, r4, pthread_equal(pthread_self(), pthread_self()));
}

static volatile sig_atomic_t signaled = 0;
static void on_signal(int number) { signaled = number; }

static void check_unistd(const char *work) {
    section("unistd");
    printf("%d %d %d %ld %ld\n", getpid() == getpid(), getpid() != getppid(), getuid() == geteuid(), sysconf(_SC_PAGESIZE), (long)getpagesize());
    int pipes[2];
    int piped = pipe(pipes);
    ssize_t wrote = write(pipes[1], "through the pipe", 16);
    char buffer[64];
    ssize_t got = read(pipes[0], buffer, sizeof buffer);
    buffer[got] = 0;
    printf("%d %zd %zd %s %d %d\n", piped, wrote, got, buffer, close(pipes[0]), close(pipes[1]));
    umask(022);
    char path[PATH_MAX];
    snprintf(path, sizeof path, "%s/raw.bin", work);
    int fd = open(path, O_CREAT | O_WRONLY | O_TRUNC, 0664);
    wrote = write(fd, "0123456789", 10);
    long long sought = lseek(fd, 3, SEEK_SET);
    ssize_t wrote_more = write(fd, "XY", 2);
    int closed = close(fd);
    fd = open(path, O_RDONLY);
    got = read(fd, buffer, sizeof buffer);
    buffer[got] = 0;
    printf("%zd %lld %zd %d %zd %s %d\n", wrote, sought, wrote_more, closed, got, buffer, close(fd));
    struct stat info;
    int stated = stat(path, &info);
    printf("%d %lld %d %o\n", stated, (long long)info.st_size, S_ISREG(info.st_mode), (unsigned)(info.st_mode & 0777));
    int readable = access(path, R_OK);
    int executable = access(path, X_OK);
    int denied = errno == EACCES;
    int unlinked = unlink(path);
    int gone = access(path, F_OK);
    int missing = errno == ENOENT;
    printf("%d %d %d %d %d %d\n", readable, executable, denied, unlinked, gone, missing);
    snprintf(path, sizeof path, "%s/dir", work);
    int made = mkdir(path, 0755);
    int again = mkdir(path, 0755);
    int exists = errno == EEXIST;
    printf("%d %d %d %d\n", made, again, exists, rmdir(path));
    int changed = chdir(work);
    printf("%d %d %d\n", changed, getcwd(buffer, sizeof buffer) != NULL, isatty(1));
    struct utsname name;
    uname(&name);
    printf("%s %d\n", name.sysname, strlen(name.release) > 0);
    pid_t child = fork();
    if (child == 0) _exit(7);
    int status = 0;
    waitpid(child, &status, 0);
    printf("%d %d\n", WIFEXITED(status), WEXITSTATUS(status));
    char *argv[] = {"prog", "-a", "-b", "value", "rest", NULL};
    int option;
    optind = 1;
    while ((option = getopt(5, argv, "ab:")) != -1) printf("%c:%s ", option, optarg ? optarg : "-");
    printf("| %d %s\n", optind, argv[optind]);
    signal(SIGUSR1, on_signal);
    raise(SIGUSR1);
    int first = signaled;
    struct sigaction action;
    memset(&action, 0, sizeof action);
    action.sa_handler = on_signal;
    sigemptyset(&action.sa_mask);
    sigaction(SIGUSR2, &action, NULL);
    kill(getpid(), SIGUSR2);
    printf("%d %d\n", first, (int)signaled);
}

static int saw_plugin = 0;
static int note_module(struct dl_phdr_info *info, size_t size, void *data) {
    (void)size;
    (void)data;
    if (info->dlpi_name && strstr(info->dlpi_name, "libmlxplugin") && info->dlpi_phnum > 0) saw_plugin++;
    return 0;
}

static void check_dl(const char *plugin) {
    section("dlfcn");
    void *handle = dlopen(plugin, RTLD_NOW);
    if (!handle) {
        printf("dlopen failed: %s\n", dlerror());
        return;
    }
    long (*sum)(long) = (long (*)(long))dlsym(handle, "mlx_plugin_sum");
    long (*twice)(long) = (long (*)(long))dlsym(handle, "mlx_plugin_twice");
    unsigned long (*count)(void) = (unsigned long (*)(void))dlsym(handle, "mlx_plugin_count");
    unsigned long *calls = (unsigned long *)dlsym(handle, "mlx_plugin_calls");
    unsigned *level = (unsigned *)dlsym(handle, "mlx_plugin_level");
    printf("%d %d %d %d %d\n", sum != NULL, twice != NULL, count != NULL, calls != NULL, level != NULL);
    if (!sum || !twice || !count || !calls || !level) return;
    printf("%ld %ld %u\n", sum(10), twice(21), *level);
    unsigned long before = *calls;
    count();
    count();
    printf("%lu %lu\n", before, *calls);
    Dl_info info;
    int found = dladdr((void *)sum, &info);
    printf("%d %s %s %d\n", found, found && info.dli_sname ? info.dli_sname : "?", found && info.dli_fname ? strrchr(info.dli_fname, '/') + 1 : "?", found && info.dli_saddr == (void *)sum);
    printf("%d %d\n", dlsym(RTLD_DEFAULT, "strlen") != NULL, dlsym(handle, "no_such_symbol") == NULL);
    void *missing = dlopen("libmlxc-no-such-library.so.9", RTLD_NOW);
    const char *message = dlerror();
    printf("%d %d %d\n", missing == NULL, message != NULL, dlerror() == NULL);
    dl_iterate_phdr(note_module, NULL);
    printf("%d %d\n", saw_plugin, dlclose(handle));
    // (Whether dlclose unmapped the plugin, and so whether its counter
    // starts over, is the loader's choice; only the handle is compared.)
    void *again = dlopen(plugin, RTLD_NOW);
    printf("%d %d\n", again == handle, dlsym(again, "mlx_plugin_calls") != NULL);
    dlclose(again);
}

static void say_goodbye(void) { printf("atexit ran\n"); }

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: mlxlibc_check WORKDIR mlx|glibc\n");
        return 2;
    }
    int on_mlx = mlxlibc_version != NULL;
    if (on_mlx != (strcmp(argv[2], "mlx") == 0)) {
        fprintf(stderr, "mlxlibc_check: running on %s, expected %s\n", on_mlx ? "mlxlibc" : "glibc", argv[2]);
        return 2;
    }
    atexit(say_goodbye);
    check_printf();
    check_strings();
    check_ctype();
    check_stdlib();
    check_malloc();
    check_stdio(argv[1]);
    check_time();
    check_pthread();
    check_unistd(argv[1]);
    if (argc > 3) check_dl(argv[3]);
    // exit, not a return from main: glibc's start code calls its own exit,
    // which knows nothing of mlxlibc's atexit handlers and stdout buffer.
    exit(0);
}
