#ifndef _SYS_SYSMACROS_H
#define _SYS_SYSMACROS_H

#include <mlibc-config.h>

#include <abi-bits/dev_t.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __MLIBC_ABI_ONLY

static inline unsigned int major(dev_t __dev) {
	return (unsigned int)(((__dev >> 8) & 0xffu) | ((__dev >> 12) & ~0xffu));
}

static inline unsigned int minor(dev_t __dev) {
	return (unsigned int)((__dev & 0xffu) | ((__dev >> 8) & ~0xffu));
}

static inline dev_t makedev(unsigned int __major, unsigned int __minor) {
	return (dev_t)((__minor & 0xffu) | ((__major & 0xffu) << 8) |
			((__minor & ~0xffu) << 12) | ((__major & ~0xffu) << 32));
}

#endif /* !__MLIBC_ABI_ONLY */

#ifdef __cplusplus
}
#endif

#endif /* _SYS_SYSMACROS_H */
