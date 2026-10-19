#ifndef _ABIBITS_IOCTLS_H
#define _ABIBITS_IOCTLS_H

#include <mlibc-config.h>

/* Terminal ioctls (asm-generic/ioctls.h) */
#define TIOCNOTTY	0x5422
#define TIOCGPTN	0x80045430
#define TIOCSPTLCK	0x40045431

/* Socket ioctls */
#define SIOCGIFCONF	0x8912

#endif /* _ABIBITS_IOCTLS_H */