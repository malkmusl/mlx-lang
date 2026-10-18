/* GNU C forms libpas (bmalloc) uses, as clang compiles them: compound
   literals initializing static objects and their members read in
   constant expressions (a configuration macro naming another's
   fields), the typed overflow builtins, an instruction given inputs it
   does not name, and Valgrind's client request (answering the default
   when not run under Valgrind). */
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <limits.h>

typedef struct { int futex; const char *name; int *p; } lock_t;
static int cell = 5;
#define LOCK_INIT(n) ((lock_t){ .futex = 7, .name = n, .p = &cell })
lock_t a = LOCK_INIT("a");
static lock_t b = (lock_t){ 1, "b", 0 };
struct outer { int x; lock_t l; } o = { 3, LOCK_INIT("inner") };

typedef struct { void (*activate)(void); bool check; size_t shift; double handicap; float ratio; short mode; } args_t;
typedef struct cfg { const struct cfg *config_ptr; int kind; void (*activate_callback)(void); bool check; size_t shift; double handicap; float ratio; short mode; } cfg_t;
static void act(void) { puts("activated"); }
#define ARGS ((args_t){ .activate = act, .check = 1, .shift = ((size_t)1 << 14), .handicap = 1.05, .ratio = 0.5f, .mode = -2 })
extern const cfg_t config;
#define CONFIG ((cfg_t){ .config_ptr = &config, .kind = 3, .activate_callback = ARGS.activate, .check = ARGS.check, .shift = ARGS.shift, .handicap = ARGS.handicap, .ratio = ARGS.ratio, .mode = ARGS.mode })
const cfg_t config = CONFIG;
const cfg_t *table[] = { (CONFIG).config_ptr };
int kinds[] = { (CONFIG).kind, (CONFIG).mode };

static void trap_unless(int ok, size_t reason)
{
    if (ok) return;
    register size_t reasonGPR __asm__("r11") = reason;
    __asm__ volatile ("int3" : : "r"(reasonGPR));
}

static unsigned long client_request(unsigned long def)
{
    volatile unsigned long args[6] = { 0x1001, 0, 0, 0, 0, 0 };
    volatile unsigned long result;
    __asm__ volatile("rolq $3,  %%rdi ; rolq $13, %%rdi\n\t"
                     "rolq $61, %%rdi ; rolq $51, %%rdi\n\t"
                     "xchgq %%rbx,%%rbx"
                     : "=d" (result)
                     : "a" (&args[0]), "0" (def)
                     : "cc", "memory");
    return result;
}

int main(void)
{
    printf("%d %s %d | %d %s %d | %d %d %s %d\n", a.futex, a.name, *a.p, b.futex, b.name, b.p == 0, o.x, o.l.futex, o.l.name, *o.l.p);
    config.activate_callback();
    printf("%d %d %zu %.2f %.1f %d %d %d %d\n", config.kind, config.check, config.shift, config.handicap, config.ratio, config.mode, table[0] == &config, kinds[0], kinds[1]);

    unsigned long long r; long l; int i; unsigned u;
    int x = __builtin_usubll_overflow(3ULL, 5ULL, &r);
    int y = __builtin_saddl_overflow(LONG_MAX, 1L, &l);
    int z = __builtin_smul_overflow(1000, 1000, &i);
    int w = __builtin_uadd_overflow(UINT_MAX, 1u, &u);
    printf("%d %llu %d %ld %d %d %d %u\n", x, r, y, l, z, i, w, u);

    trap_unless(1, 42);
    printf("%lu %lu\n", client_request(0), client_request(42));
    return 0;
}
