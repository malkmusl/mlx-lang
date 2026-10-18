/* -include FILE (forcedinclude.flags): the header is read first, as if
   included on the first line; included again here, #pragma once makes
   it nothing however it is reached. And a macro whose expansion ends
   with an empty function-like one leaves the next line's directive one
   (JavaScriptCore's FOR_EACH lists). */
#include <stdio.h>
#include "forcedinclude.h"
#include "./forcedinclude.h"

#define EMPTY_LIST(v)
#define LIST(v) \
    v(alpha, 1) \
    v(beta, 2) \
    EMPTY_LIST(v)
#define DECLARE(name, value) static int name = value;
LIST(DECLARE)
#undef DECLARE
#ifdef DECLARE
#error DECLARE is still defined
#endif

int main(void)
{
    forced_count++;
    printf("%s %d %d %d\n", FORCED_GREETING, forced_count, alpha, beta);
    return 0;
}
