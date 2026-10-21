#ifndef _MLXCC_SYS_IOCTL_H
#define _MLXCC_SYS_IOCTL_H
struct winsize { unsigned short ws_row; unsigned short ws_col; unsigned short ws_xpixel; unsigned short ws_ypixel; };
#define TCGETS 0x5401
#define TCSETS 0x5402
#define TCSETSW 0x5403
#define TCSETSF 0x5404
#define TIOCGPGRP 0x540F
#define TIOCSPGRP 0x5410
#define TIOCOUTQ 0x5411
#define TIOCGWINSZ 0x5413
#define TIOCSWINSZ 0x5414
#define FIONREAD 0x541B
#define TIOCINQ FIONREAD
#define FIONBIO 0x5421
#define FIOCLEX 0x5451
#define FIONCLEX 0x5450
int ioctl(int, unsigned long, ...);
#endif
