#include <cstddef>
#include <cstdint>
#include <string.h>
#include <sys/select.h>
#include <time.h>
#include <unistd.h>

#include <mlibc/all-sysdeps.hpp>
#include <mlibc/threads.hpp>

#ifndef MLIBC_STATIC_BUILD

extern "C" [[gnu::visibility("hidden")]] char *strncpy(char *dest, const char *src, size_t n) {
    size_t i = 0;
    for (; i < n && src[i]; i++) dest[i] = src[i];
    for (; i < n; i++) dest[i] = 0;
    return dest;
}

extern "C" [[gnu::visibility("hidden")]] void __FD_SET(int fd, fd_set *set) {
    if (fd >= 0 && fd < FD_SETSIZE) set->fds_bits[fd / 32] |= (1UL << (fd % 32));
}

extern "C" [[gnu::visibility("hidden")]] void __FD_CLR(int fd, fd_set *set) {
    if (fd >= 0 && fd < FD_SETSIZE) set->fds_bits[fd / 32] &= ~(1UL << (fd % 32));
}

extern "C" [[gnu::visibility("hidden")]] void __FD_ZERO(fd_set *set) {
    for (size_t i = 0; i < sizeof(fd_set) / sizeof(long); i++) set->fds_bits[i] = 0;
}

extern "C" [[gnu::visibility("hidden")]] int __FD_ISSET(int fd, fd_set *set) {
    return fd >= 0 && fd < FD_SETSIZE && (set->fds_bits[fd / 32] & (1UL << (fd % 32)));
}

extern "C" [[gnu::visibility("hidden")]] pid_t getpid(void) {
    return mlibc::SysdepImpl<GetPid>::operator()();
}

extern "C" [[gnu::visibility("hidden")]] int clock_gettime(int clk, struct timespec *ts) {
    return mlibc::sysdep_or_enosys<ClockGet>(clk, &ts->tv_sec, &ts->tv_nsec);
}

namespace mlibc {
[[gnu::visibility("hidden")]] void thread_exit(thread_exit_return ret_val) {
    sysdep_or_panic<ThreadExit>();
    __builtin_unreachable();
}
}

#endif
