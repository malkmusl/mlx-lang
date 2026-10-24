// C beside the C++: a struct, a static function, a macro, a global.
#include "shapes_api.h"

#define LIMIT 10

struct counter {
    int value;
};

int counted = 0;

static void bump(struct counter *c) {
    c->value = c->value + 1;
    counted = counted + 1;
}

int legacy_run(void) {
    struct counter c = { 0 };
    while (c.value < LIMIT) bump(&c);
    return shapes_scale(c.value);
}
