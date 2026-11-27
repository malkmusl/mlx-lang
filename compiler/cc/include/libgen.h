/* <libgen.h>: basename and dirname as POSIX has them (they may change
   the path they are given). */
#ifndef _MLXCC_LIBGEN_H
#define _MLXCC_LIBGEN_H
#ifdef __cplusplus
extern "C" {
#endif
char *basename(char *);
char *dirname(char *);
#ifdef __cplusplus
}
#endif
#endif
