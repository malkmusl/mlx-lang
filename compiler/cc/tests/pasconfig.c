// libpas's shape: a 2 KiB configuration of constants and function
// addresses built as a compound literal, passed by value down a chain of
// always_inline functions that read a few fields of it. The fields are
// to fold to the constants and the literal and the parameters' copies
// are not to be made (region_loads.mlx; mlx-cc built the whole literal
// on the stack at every call, 12 µs an allocation in bmalloc). A double
// in the configuration is passed to a real call by value as a word of
// the integer class (a folded load keeps the load's class).
#include <stdio.h>
#include <stddef.h>
#define ALWAYS_INLINE static inline __attribute__((always_inline))

typedef struct {
    unsigned kind;
    unsigned page_size;
    double handicap;
    size_t (*object_size)(unsigned);
    unsigned (*header_size)(void);
    unsigned long table[120];
} page_config;

typedef struct {
    unsigned heap_kind;
    page_config small;
    page_config medium;
    const char* name;
} heap_config;

static size_t small_object_size(unsigned index) { return 16 * (index + 1); }
static unsigned small_header_size(void) { return 64; }
static size_t medium_object_size(unsigned index) { return 256 * (index + 1); }
static unsigned medium_header_size(void) { return 128; }

#define SMALL_CONFIG ((page_config){ .kind = 1, .page_size = 16384, .handicap = 1.5, .object_size = small_object_size, .header_size = small_header_size, .table = { 3, 5, 7 } })
#define MEDIUM_CONFIG ((page_config){ .kind = 2, .page_size = 131072, .handicap = 0.25, .object_size = medium_object_size, .header_size = medium_header_size, .table = { 11, 13 } })
#define HEAP_CONFIG ((heap_config){ .heap_kind = 7, .small = SMALL_CONFIG, .medium = MEDIUM_CONFIG, .name = "heap" })

ALWAYS_INLINE unsigned long page_header_for_boundary(unsigned long boundary, page_config config)
{
    return boundary + config.page_size - config.header_size() + config.table[config.kind];
}

__attribute__((noinline)) static unsigned long refill(unsigned long boundary, page_config config)
{
    return boundary + (unsigned long)(config.handicap * config.page_size) + config.table[1];
}

ALWAYS_INLINE unsigned long allocate_in_page(unsigned long boundary, unsigned index, page_config config)
{
    if ((index & 3) == 3)
        return refill(boundary, config);
    return page_header_for_boundary(boundary, config) + config.object_size(index);
}

ALWAYS_INLINE unsigned long allocate_impl(unsigned long boundary, unsigned index, heap_config config)
{
    if (index < 8)
        return allocate_in_page(boundary, index, config.small) + config.heap_kind;
    return allocate_in_page(boundary, index, config.medium) + config.heap_kind;
}

unsigned long allocate(unsigned long boundary, unsigned index)
{
    return allocate_impl(boundary, index, HEAP_CONFIG);
}

int main(void)
{
    unsigned long sum = 0;
    for (unsigned i = 0; i < 20; i++)
        sum += allocate(4096 * i, i);
    printf("%lu\n", sum);
    return 0;
}
