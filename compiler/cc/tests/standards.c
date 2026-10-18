/* What each -std= variant predefines: __STDC_VERSION__ (or none),
   __STRICT_ANSI__, and `linux` and `unix` (GNU variants). check_cc.sh
   compiles this with every variant and compares the line. */
#include <stdio.h>
int main(void)
{
#ifdef __STDC_VERSION__
    printf("%ld", (long)__STDC_VERSION__);
#else
    printf("none");
#endif
#ifdef __STRICT_ANSI__
    printf(" strict");
#else
    printf(" gnu");
#endif
#if defined(linux) && defined(unix)
    printf(" linux unix\n");
#else
    printf(" no-linux-unix\n");
#endif
    return 0;
}
