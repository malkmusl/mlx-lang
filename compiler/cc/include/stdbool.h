#ifndef _MLXCC_STDBOOL_H
#define _MLXCC_STDBOOL_H
/* (C++ and C23 have bool, true and false as keywords.) */
#if !defined(__cplusplus) && (!defined(__STDC_VERSION__) || __STDC_VERSION__ < 202311L)
#define bool _Bool
#define true 1
#define false 0
#endif
#define __bool_true_false_are_defined 1
#endif
