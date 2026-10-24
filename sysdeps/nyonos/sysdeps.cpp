#include <bits/syscall.h>
#include <bits/nyonos-syscall.h>
#include <bits/ensure.h>
#include <mlibc/all-sysdeps.hpp>

#include <abi-bits/errno.h>
#include <abi-bits/mode_t.h>
#include <abi-bits/fcntl.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stddef.h>
#include <sys/stat.h>
#include <mlibc/tcb.hpp>

#include <time.h>
#include <string.h>
#include <sys/uio.h>
#include <sys/times.h>
#include <sys/resource.h>
#include <sched.h>
#include <unistd.h>

namespace mlibc {

struct NyonosDirent {
	uint64_t size;
	uint32_t cluster;
	uint32_t attr;
	uint32_t is_dir;
	char name[32];
};

static int sc_error(long ret) {
	if (ret < 0 && ret >= -4095)
		return static_cast<int>(-ret);
	return 0;
}

void Sysdeps<LibcLog>::operator()(const char *message) {
	long unused;
	syscall(SYS_WRITE, 2, message, strlen(message), &unused);
}

void Sysdeps<LibcPanic>::operator()() {
	sysdep<LibcLog>("!!! mlibc panic !!!");
	sysdep<Exit>(-1);
	__builtin_trap();
}

namespace {

struct sigaction saved_actions[65];
sigset_t saved_mask;

}

int Sysdeps<Sigaction>::operator()(int sig, const struct sigaction *__restrict act,
		struct sigaction *__restrict old) {
	if (sig <= 0 || sig >= 65) return EINVAL;
	if (old) {
		struct sigaction cur = saved_actions[sig];
		cur.sa_restorer = nullptr;
		*old = cur;
	}
	if (act) saved_actions[sig] = *act;
	return 0;
}

int Sysdeps<Sigprocmask>::operator()(int how, const sigset_t *__restrict set,
		sigset_t *__restrict retrieve) {
	if (retrieve) *retrieve = saved_mask;
	if (!set) return 0;
	switch (how) {
	case SIG_BLOCK:
		saved_mask.__sig[0] |= set->__sig[0];
		break;
	case SIG_UNBLOCK:
		saved_mask.__sig[0] &= ~set->__sig[0];
		break;
	case SIG_SETMASK:
		saved_mask.__sig[0] = set->__sig[0];
		break;
	default:
		return EINVAL;
	}
	return 0;
}

int Sysdeps<Execve>::operator()(const char *path, char *const argv[], char *const envp[]) {
	return sc_error(syscall(SYS_EXECVE, reinterpret_cast<__sc_word_t>(path),
				reinterpret_cast<__sc_word_t>(argv),
				reinterpret_cast<__sc_word_t>(envp)));
}

void Sysdeps<Exit>::operator()(int status) {
	syscall(SYS_EXIT, status);
	__builtin_unreachable();
}

int Sysdeps<Write>::operator()(int fd, const void *buf, size_t size, ssize_t *bytes_written) {
	auto ret = syscall(SYS_WRITE, fd, reinterpret_cast<__sc_word_t>(buf), size);
	if (int e = sc_error(ret))
		return e;
	*bytes_written = static_cast<ssize_t>(ret);
	return 0;
}

int Sysdeps<Read>::operator()(int fd, void *buf, size_t size, ssize_t *bytes_read) {
	auto ret = syscall(SYS_READ, fd, reinterpret_cast<__sc_word_t>(buf), size);
	if (int e = sc_error(ret))
		return e;
	*bytes_read = static_cast<ssize_t>(ret);
	return 0;
}

int Sysdeps<Pread>::operator()(int fd, void *buf, size_t size, off_t off, ssize_t *bytes_read) {
	auto ret = syscall(SYS_PREAD, fd, reinterpret_cast<__sc_word_t>(buf), size,
			reinterpret_cast<__sc_word_t>(off));
	if (int e = sc_error(ret))
		return e;
	*bytes_read = static_cast<ssize_t>(ret);
	return 0;
}

int Sysdeps<Close>::operator()(int fd) {
	return sc_error(syscall(SYS_CLOSE, fd));
}

int Sysdeps<Seek>::operator()(int fd, off_t offset, int whence, off_t *new_offset) {
	auto ret = syscall(SYS_LSEEK, fd, reinterpret_cast<__sc_word_t>(offset), whence);
	if (int e = sc_error(ret))
		return e;
	*new_offset = static_cast<off_t>(ret);
	return 0;
}

int Sysdeps<Isatty>::operator()(int fd) {
	auto ret = syscall(SYS_ISATTY, fd);
	if (int e = sc_error(ret))
		return e;
	return ret ? 0 : ENOTTY;
}

int Sysdeps<ClockGet>::operator()(int clock, time_t *secs, long *nanos) {
	struct timespec ts;
	auto ret = syscall(SYS_CLOCK_GETTIME, clock, reinterpret_cast<__sc_word_t>(&ts));
	if (int e = sc_error(ret))
		return e;
	*secs = ts.tv_sec;
	*nanos = ts.tv_nsec;
	return 0;
}

int Sysdeps<TcbSet>::operator()(void *pointer) {
	syscall(SYS_SET_FS_BASE, reinterpret_cast<__sc_word_t>(pointer));
	return 0;
}

int Sysdeps<FutexWait>::operator()(int *pointer, int expected, const struct timespec *time) {
	auto ret = syscall(SYS_FUTEX, reinterpret_cast<__sc_word_t>(pointer), 0u,
			static_cast<__sc_word_t>(expected), reinterpret_cast<__sc_word_t>(time));
	return sc_error(ret);
}

void Sysdeps<Yield>::operator()() {
	syscall(SYS_YIELD, 0);
}

int Sysdeps<FutexWake>::operator()(int *pointer, bool all) {
	auto ret = syscall(SYS_FUTEX, reinterpret_cast<__sc_word_t>(pointer), 1u, 0, 0);
	return sc_error(ret);
}

int Sysdeps<GetPgid>::operator()(pid_t pid, pid_t *pgid) {
	(void)pid;
	*pgid = static_cast<pid_t>(syscall(SYS_GETPID));
	return 0;
}

int Sysdeps<GetSid>::operator()(pid_t pid, pid_t *sid) {
	(void)pid;
	*sid = static_cast<pid_t>(syscall(SYS_GETPID));
	return 0;
}

int Sysdeps<SetPgid>::operator()(pid_t pid, pid_t pgid) {
	(void)pid;
	(void)pgid;
	return 0;
}

int Sysdeps<SetSid>::operator()(pid_t *sid) {
	*sid = static_cast<pid_t>(syscall(SYS_GETPID));
	return 0;
}

static const uint16_t NYO_POLLIN  = 0x001;
static const uint16_t NYO_POLLOUT = 0x004;
static const uint16_t NYO_POLLNVAL = 0x020;

int Sysdeps<Poll>::operator()(struct pollfd *fds, nfds_t count, int timeout,
                              int *num_events) {
	auto ret = syscall(SYS_POLL, reinterpret_cast<__sc_word_t>(fds),
	                    static_cast<__sc_word_t>(count),
	                    static_cast<__sc_word_t>(timeout),
	                    reinterpret_cast<__sc_word_t>(num_events));
	if (sc_error(ret))
		return sc_error(ret);
	return static_cast<int>(ret);
}

int Sysdeps<Pselect>::operator()(int num_fds, fd_set *read_set, fd_set *write_set,
                                 fd_set *except_set, const struct timespec *timeout,
                                 const sigset_t *sigmask, int *num_events) {
	(void)sigmask;

	int64_t timeout_ms = -1;
	if (timeout) {
		timeout_ms = static_cast<int64_t>(timeout->tv_sec) * 1000
		           + timeout->tv_nsec / 1000000;
	}

	if (num_fds > 1024)
		num_fds = 1024;

	struct pollfd pfds[64];
	int n = num_fds > 64 ? 64 : num_fds;
	for (int i = 0; i < n; i++) {
		pfds[i].fd = i;
		pfds[i].events = 0;
		pfds[i].revents = 0;

		if (read_set && FD_ISSET(i, read_set))
			pfds[i].events |= NYO_POLLIN;
		if (write_set && FD_ISSET(i, write_set))
			pfds[i].events |= NYO_POLLOUT;
	}

	int ready = 0;
	auto ret = syscall(SYS_POLL, reinterpret_cast<__sc_word_t>(pfds),
	                    static_cast<__sc_word_t>(n),
	                    static_cast<__sc_word_t>(timeout_ms),
	                    reinterpret_cast<__sc_word_t>(num_events));
	if (!sc_error(ret))
		ready = static_cast<int>(ret);

	if (read_set)
		FD_ZERO(read_set);
	if (write_set)
		FD_ZERO(write_set);

	for (int i = 0; i < n; i++) {
		uint16_t rev = pfds[i].revents;
		if (rev & NYO_POLLIN) {
			if (read_set) FD_SET(i, read_set);
		}
		if (rev & NYO_POLLOUT) {
			if (write_set) FD_SET(i, write_set);
		}
	}

	if (num_events)
		*num_events = ready;
	return 0;
}

int Sysdeps<Tcgetattr>::operator()(int fd, struct termios *attr) {
	auto ret = syscall(SYS_TCGETATTR, static_cast<__sc_word_t>(fd),
	                    reinterpret_cast<__sc_word_t>(attr));
	if (sc_error(ret))
		return sc_error(ret);
	return 0;
}

int Sysdeps<Tcsetattr>::operator()(int fd, int act, const struct termios *attr) {
	auto ret = syscall(SYS_TCSETATTR, static_cast<__sc_word_t>(fd),
	                    static_cast<__sc_word_t>(act),
	                    reinterpret_cast<__sc_word_t>(attr));
	if (sc_error(ret))
		return sc_error(ret);
	return 0;
}

int Sysdeps<Tcgetwinsize>::operator()(int fd, struct winsize *winsz) {
	auto ret = syscall(SYS_TCGETWINSIZE, static_cast<__sc_word_t>(fd),
	                    reinterpret_cast<__sc_word_t>(winsz));
	if (sc_error(ret))
		return sc_error(ret);
	return 0;
}

int Sysdeps<Tcsetwinsize>::operator()(int fd, const struct winsize *winsz) {
	auto ret = syscall(SYS_TCSETWINSIZE, static_cast<__sc_word_t>(fd),
	                    reinterpret_cast<__sc_word_t>(winsz));
	if (sc_error(ret))
		return sc_error(ret);
	return 0;
}

int Sysdeps<Ttyname>::operator()(int fd, char *buf, size_t size) {
	auto ret = syscall(SYS_TTYNAME, static_cast<__sc_word_t>(fd),
	                    reinterpret_cast<__sc_word_t>(buf),
	                    static_cast<__sc_word_t>(size));
	if (sc_error(ret))
		return sc_error(ret);
	return 0;
}

pid_t Sysdeps<GetPpid>::operator()() {
	auto ret = syscall(SYS_GETPPID);
	if (sc_error(ret))
		return ret;
	return static_cast<pid_t>(ret);
}

uid_t Sysdeps<GetUid>::operator()() {
	auto ret = syscall(SYS_GETUID);
	if (sc_error(ret))
		return ret;
	return static_cast<uid_t>(ret);
}

gid_t Sysdeps<GetGid>::operator()() {
	auto ret = syscall(SYS_GETGID);
	if (sc_error(ret))
		return ret;
	return static_cast<gid_t>(ret);
}

int Sysdeps<GetResuid>::operator()(uid_t *ruid, uid_t *euid, uid_t *suid) {
	auto ret = syscall(SYS_GETRESUID, reinterpret_cast<__sc_word_t>(ruid),
	                    reinterpret_cast<__sc_word_t>(euid),
	                    reinterpret_cast<__sc_word_t>(suid));
	if (sc_error(ret))
		return sc_error(ret);
	return 0;
}

int Sysdeps<GetResgid>::operator()(gid_t *rgid, gid_t *egid, gid_t *sgid) {
	auto ret = syscall(SYS_GETRESGID, reinterpret_cast<__sc_word_t>(rgid),
	                    reinterpret_cast<__sc_word_t>(egid),
	                    reinterpret_cast<__sc_word_t>(sgid));
	if (sc_error(ret))
		return sc_error(ret);
	return 0;
}

int Sysdeps<SetHostname>::operator()(const char *name, size_t len) {
	auto ret = syscall(SYS_SETHOSTNAME, reinterpret_cast<__sc_word_t>(name),
	                    static_cast<__sc_word_t>(len));
	if (sc_error(ret))
		return sc_error(ret);
	return 0;
}

int Sysdeps<GetHostname>::operator()(char *name, size_t len) {
	auto ret = syscall(SYS_GETHOSTNAME, reinterpret_cast<__sc_word_t>(name),
	                    static_cast<__sc_word_t>(len));
	if (sc_error(ret))
		return sc_error(ret);
	return 0;
}

pid_t Sysdeps<GetPid>::operator()() {
	auto ret = syscall(SYS_GETPID);
	if (sc_error(ret))
		return ret;
	return static_cast<pid_t>(ret);
}

int Sysdeps<Dup>::operator()(int fd, int flags, int *newfd) {
	auto ret = syscall(SYS_DUP, fd, flags, 0);
	if (int e = sc_error(ret))
		return e;
	*newfd = static_cast<int>(ret);
	return 0;
}

int Sysdeps<Fcntl>::operator()(int fd, int request, va_list args, int *result) {
	intptr_t arg;
	switch (request) {
	case F_DUPFD:
	case F_SETFD:
		arg = static_cast<intptr_t>(va_arg(args, int));
		break;
	case F_GETFD:
	case F_GETFL:
		arg = 0;
		break;
	default:
		arg = reinterpret_cast<intptr_t>(va_arg(args, void *));
		break;
	}

	auto ret = syscall(SYS_FCNTL, fd, request, static_cast<__sc_word_t>(arg));
	if (int e = sc_error(ret))
		return e;
	*result = static_cast<int>(ret);
	return 0;
}

int Sysdeps<Ioctl>::operator()(int fd, unsigned long request, void *arg, int *result) {
	auto ret = syscall(SYS_IOCTL, fd, request, reinterpret_cast<__sc_word_t>(arg));
	if (int e = sc_error(ret))
		return e;
	*result = static_cast<int>(ret);
	return 0;
}

static const int dir_slots = 16;
static int dir_handle[dir_slots];
static unsigned dir_index[dir_slots];

static unsigned dir_seek(int handle) {
	for (int i = 0; i < dir_slots; i++)
		if (dir_handle[i] == handle)
			return dir_index[i];
	return 0;
}

static void dir_place(int handle, unsigned index) {
	for (int i = 0; i < dir_slots; i++) {
		if (dir_handle[i] == handle || dir_handle[i] == 0) {
			dir_handle[i] = handle;
			dir_index[i] = index;
			return;
		}
	}
}

int Sysdeps<OpenDir>::operator()(const char *path, int *handle) {
	auto ret = syscall(SYS_OPEN, reinterpret_cast<__sc_word_t>(path), O_RDONLY | O_DIRECTORY);
	if (int e = sc_error(ret))
		return e;
	*handle = static_cast<int>(ret);
	dir_place(*handle, 0);
	return 0;
}

int Sysdeps<ReadEntries>::operator()(int handle, void *buffer, size_t max_size, size_t *bytes_read) {
	struct NyonosDirent dent;
	auto out = reinterpret_cast<char *>(buffer);
	size_t total = 0;
	unsigned index = dir_seek(handle);

	for (;;) {
		auto ret = syscall(SYS_GETDENTS, handle, index, reinterpret_cast<__sc_word_t>(&dent));
		if (ret <= 0) {
			if (int e = sc_error(ret))
				return e;
			break;
		}

		size_t namelen = 0;
		while (namelen < sizeof(dent.name) && dent.name[namelen])
			namelen++;
		if (namelen == 0)
			break;

		size_t reclen = offsetof(struct dirent, d_name) + namelen + 1;
		reclen = (reclen + 7) & ~static_cast<size_t>(7);
		if (total + reclen > max_size)
			break;

		struct dirent *d = reinterpret_cast<struct dirent *>(out + total);
		memset(d, 0, reclen);
		d->d_ino = dent.cluster;
		d->d_off = index + 1;
		d->d_reclen = reclen;
		d->d_type = dent.is_dir ? 4 : 8;
		for (size_t i = 0; i < namelen; i++)
			d->d_name[i] = dent.name[i];

		total += reclen;
		index++;
	}

	dir_place(handle, index);
	*bytes_read = total;
	return 0;
}

int Sysdeps<Open>::operator()(const char *pathname, int flags, mode_t mode, int *fd) {
	auto ret = syscall(SYS_OPEN, reinterpret_cast<__sc_word_t>(pathname), flags);
	if (int e = sc_error(ret))
		return e;
	*fd = static_cast<int>(ret);
	return 0;
}

int Sysdeps<AnonAllocate>::operator()(size_t size, void **pointer) {
	auto ret = syscall(SYS_MMAP, static_cast<__sc_word_t>(size),
			ABI_MMAP_PROT_READ | ABI_MMAP_PROT_WRITE, ABI_MMAP_ANON);
	if (int e = sc_error(ret))
		return e;
	*pointer = reinterpret_cast<void *>(static_cast<__sc_word_t>(ret));
	return 0;
}

int Sysdeps<AnonFree>::operator()(void *pointer, size_t size) {
	auto ret = syscall(SYS_MUNMAP, reinterpret_cast<__sc_word_t>(pointer),
			static_cast<__sc_word_t>(size));
	return sc_error(ret);
}

int Sysdeps<Pipe>::operator()(int *fds, int flags) {
	(void)flags;

	auto ret = syscall(SYS_PIPE, reinterpret_cast<__sc_word_t>(fds));
	if (sc_error(ret))
		return sc_error(ret);
	return 0;
}

int Sysdeps<VmProtect>::operator()(void *pointer, size_t size, int prot) {
	unsigned kprot = 0;
	if (prot & PROT_READ)
		kprot |= ABI_MMAP_PROT_READ;
	if (prot & PROT_WRITE)
		kprot |= ABI_MMAP_PROT_WRITE;
	if (prot & PROT_EXEC)
		kprot |= ABI_MMAP_PROT_EXEC;

	auto ret = syscall(SYS_MPROTECT, reinterpret_cast<__sc_word_t>(pointer),
	                    static_cast<__sc_word_t>(size), static_cast<__sc_word_t>(kprot));
	if (sc_error(ret))
		return sc_error(ret);
	return 0;
}

int Sysdeps<VmMap>::operator()(void *hint, size_t size, int prot, int flags, int fd,
		off_t offset, void **window) {
	unsigned kprot = 0;
	if (prot & PROT_READ)
		kprot |= ABI_MMAP_PROT_READ;
	if (prot & PROT_WRITE)
		kprot |= ABI_MMAP_PROT_WRITE;
	if (prot & PROT_EXEC)
		kprot |= ABI_MMAP_PROT_EXEC;

	auto ret = syscall(SYS_MMAP, static_cast<__sc_word_t>(size), kprot, ABI_MMAP_ANON);
	if (int e = sc_error(ret))
		return e;
	*window = reinterpret_cast<void *>(static_cast<__sc_word_t>(ret));
	return 0;
}

int Sysdeps<VmUnmap>::operator()(void *pointer, size_t size) {
	auto ret = syscall(SYS_MUNMAP, reinterpret_cast<__sc_word_t>(pointer),
			static_cast<__sc_word_t>(size));
	return sc_error(ret);
}

struct NyonosStat {
	uint64_t size;
	uint64_t cluster;
	uint32_t attr;
	uint32_t is_dir;
	char name[32];
};

struct NyonosTimespec2 {
	int64_t asec;
	int64_t ansec;
	int64_t msec;
	int64_t mnsec;
};

static void fill_linux_stat(struct stat *out, const NyonosStat *in) {
	memset(out, 0, sizeof(*out));
	out->st_dev = 0;
	out->st_ino = in->cluster;
	out->st_nlink = 1;
	out->st_mode = in->is_dir ? S_IFDIR | 0755 : S_IFREG | 0644;
	out->st_uid = 0;
	out->st_gid = 0;
	out->st_rdev = 0;
	out->st_size = static_cast<off_t>(in->size);
	out->st_blksize = 512;
	out->st_blocks = static_cast<blkcnt_t>((in->size + 511) / 512);
	out->st_atim.tv_sec = 0;
	out->st_atim.tv_nsec = 0;
	out->st_mtim.tv_sec = 0;
	out->st_mtim.tv_nsec = 0;
	out->st_ctim.tv_sec = 0;
	out->st_ctim.tv_nsec = 0;
}

int Sysdeps<Stat>::operator()(fsfd_target fsfdt, int fd, const char *path, int flags,
		struct stat *statbuf) {
	NyonosStat kst;
	__sc_word_t paddr = reinterpret_cast<__sc_word_t>(&kst);
	auto ret = (fd >= 0) ? syscall(SYS_FSTAT, fd, paddr)
			     : syscall(SYS_STAT, reinterpret_cast<__sc_word_t>(path), paddr);
	if (int e = sc_error(ret))
		return e;

	fill_linux_stat(statbuf, &kst);
	return 0;
}

int Sysdeps<Chdir>::operator()(const char *path) {
	auto ret = syscall(SYS_CHDIR, reinterpret_cast<__sc_word_t>(path));
	return sc_error(ret);
}

int Sysdeps<GetCwd>::operator()(char *buf, size_t size) {
	auto ret = syscall(SYS_GETCWD, reinterpret_cast<__sc_word_t>(buf),
	                   static_cast<__sc_word_t>(size));
	return sc_error(ret);
}

int Sysdeps<Fork>::operator()(pid_t *child) {
	auto ret = syscall(SYS_FORK, reinterpret_cast<__sc_word_t>(child));
	return sc_error(ret);
}

int Sysdeps<Waitpid>::operator()(pid_t pid, int *status, int flags, struct rusage *ru,
		pid_t *ret_pid) {
	auto ret = syscall(SYS_WAITPID, pid, reinterpret_cast<__sc_word_t>(status), flags);
	if (int e = sc_error(ret))
		return e;
	if (ret_pid)
		*ret_pid = static_cast<pid_t>(ret);
	return 0;
}

int Sysdeps<Sleep>::operator()(time_t *secs, long *nanos) {
	if (!secs)
		return EINVAL;

	long rem_nanos = nanos ? *nanos : 0;
	if (rem_nanos < 0 || rem_nanos >= 1000000000L)
		return EINVAL;

	if (*secs < 0) {
		*secs = 0;
		if (nanos)
			*nanos = 0;
		return 0;
	}

	long long total_nanos = static_cast<long long>(*secs) * 1000000000LL + rem_nanos;
	long ms = static_cast<long>((total_nanos + 999999LL) / 1000000LL);

	if (ms > 60000L)
		ms = 60000L;

	syscall(SYS_SLEEP, ms);

	long long slept_nanos = static_cast<long long>(ms) * 1000000LL;
	if (slept_nanos > total_nanos)
		slept_nanos = total_nanos;

	long long left = total_nanos - slept_nanos;
	*secs = static_cast<time_t>(left / 1000000000LL);
	if (nanos)
		*nanos = static_cast<long>(left % 1000000000LL);
	return 0;
}

int Sysdeps<Dup2>::operator()(int fd, int flags, int newfd) {
	auto ret = syscall(SYS_DUP2, fd, newfd, flags);
	return sc_error(ret);
}

int Sysdeps<Openat>::operator()(int dirfd, const char *path, int flags, mode_t mode,
		int *fd) {
	if (dirfd != ABI_AT_FDCWD)
		return ENOSYS;

	auto ret = syscall(SYS_OPEN, reinterpret_cast<__sc_word_t>(path), flags);
	if (int e = sc_error(ret))
		return e;
	*fd = static_cast<int>(ret);
	return 0;
}

#include <sys/uio.h>
#include <sys/times.h>
#include <sys/resource.h>
#include <sched.h>
#include <unistd.h>
#include <time.h>

namespace {

bool cpu_has_rdrand(void) {
	uint32_t a, b, c, d;
	__asm__ volatile("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "a"(1), "c"(0));
	return (c & (1u << 30)) != 0;
}

bool rdrand64(uint64_t *out) {
	for (int i = 0; i < 16; i++) {
		uint32_t lo, hi;
		unsigned char c1 = 0, c2 = 0;
		__asm__ volatile("rdrand %0" : "=a"(lo), "=@ccc"(c1));
		__asm__ volatile("rdrand %0" : "=a"(hi), "=@ccc"(c2));
		if (c1 && c2) {
			*out = ((uint64_t)hi << 32) | lo;
			return true;
		}
	}
	return false;
}

uint64_t fallback_entropy(void) {
	static uint64_t state;
	if (!state) {
		time_t s = 0;
		long ns = 0;
		sysdep<ClockGet>(0, &s, &ns);
		state = (uint64_t)s * 1000000000ULL + (uint64_t)ns;
		state ^= (uint64_t)(uintptr_t)&state;
		state ^= (uint64_t)getpid() << 32;
		if (!state) state = 0x9E3779B97F4A7C15ULL;
	}
	state ^= state << 13;
	state ^= state >> 7;
	state ^= state << 17;
	return state;
}

mode_t umask_value;

}

int Sysdeps<GetEntropy>::operator()(void *buffer, size_t length) {
	uint8_t *out = (uint8_t *)buffer;
	bool rdrand = cpu_has_rdrand();
	size_t done = 0;

	while (done < length) {
		uint64_t v;
		if (rdrand && rdrand64(&v)) {
			__builtin_memcpy(out + done, &v, 8);
			done += 8;
		} else {
			v = fallback_entropy();
			size_t n = length - done;
			if (n > 8) n = 8;
			__builtin_memcpy(out + done, &v, n);
			done += n;
		}
	}

	return 0;
}

int Sysdeps<Readv>::operator()(int fd, const struct iovec *iovs, int iovc, ssize_t *bytes_read) {
	if (iovc < 0) return EINVAL;
	ssize_t total = 0;

	for (int i = 0; i < iovc; i++) {
		if (iovs[i].iov_len == 0) continue;
		ssize_t n = 0;
		int e = sysdep<Read>(fd, iovs[i].iov_base, iovs[i].iov_len, &n);
		if (e) return total > 0 ? 0 : e;
		total += n;
		if ((size_t)n < iovs[i].iov_len) break;
	}

	*bytes_read = total;
	return 0;
}

int Sysdeps<Writev>::operator()(int fd, const struct iovec *iovs, int iovc, ssize_t *bytes_written) {
	if (iovc < 0) return EINVAL;
	ssize_t total = 0;

	for (int i = 0; i < iovc; i++) {
		if (iovs[i].iov_len == 0) continue;
		ssize_t n = 0;
		int e = sysdep<Write>(fd, iovs[i].iov_base, iovs[i].iov_len, &n);
		if (e) return total > 0 ? 0 : e;
		total += n;
		if ((size_t)n < iovs[i].iov_len) break;
	}

	*bytes_written = total;
	return 0;
}

int Sysdeps<Pwrite>::operator()(int fd, const void *buf, size_t n, off_t off, ssize_t *bytes) {
	off_t saved = 0;
	if (int e = sysdep<Seek>(fd, 0, SEEK_CUR, &saved)) return e;

	off_t ignored = 0;
	int e = sysdep<Seek>(fd, off, SEEK_SET, &ignored);
	if (e) {
		sysdep<Seek>(fd, saved, SEEK_SET, &ignored);
		return e;
	}

	ssize_t w = 0;
	e = sysdep<Write>(fd, buf, n, &w);
	sysdep<Seek>(fd, saved, SEEK_SET, &ignored);
	if (e) return e;

	*bytes = w;
	return 0;
}

int Sysdeps<Access>::operator()(const char *path, int mode) {
	if (mode & ~(F_OK | X_OK | W_OK | R_OK)) return EINVAL;

	struct stat st;
	if (int e = sysdep<Stat>(fsfd_target::none, -1, path, 0, &st)) return e;

	mode_t bits = st.st_mode;

	if ((mode & X_OK) && !(bits & S_IXUSR)) return EACCES;
	if ((mode & W_OK) && !(bits & S_IWUSR)) return EACCES;
	if ((mode & R_OK) && !(bits & S_IRUSR)) return EACCES;

	return 0;
}

int Sysdeps<Faccessat>::operator()(int dirfd, const char *pathname, int mode, int flags) {
	(void)flags;
	if (dirfd != ABI_AT_FDCWD) return ENOSYS;
	return sysdep<Access>(pathname, mode);
}

int Sysdeps<Sysconf>::operator()(int num, long *ret) {
	switch (num) {
	case _SC_NPROCESSORS_ONLN:
	case _SC_NPROCESSORS_CONF:
		*ret = 1;
		return 0;
	case _SC_PHYS_PAGES:
		*ret = 32768;
		return 0;
	case _SC_AVPHYS_PAGES:
		*ret = 16384;
		return 0;
	case _SC_PAGESIZE:
		*ret = 4096;
		return 0;
	case _SC_CLK_TCK:
		*ret = 100;
		return 0;
	case _SC_CHILD_MAX:
		*ret = 256;
		return 0;
	case _SC_OPEN_MAX:
		*ret = 256;
		return 0;
	case _SC_STREAM_MAX:
		*ret = 16;
		return 0;
	case _SC_ARG_MAX:
		*ret = 131072;
		return 0;
	case _SC_NGROUPS_MAX:
		*ret = 1;
		return 0;
	case _SC_JOB_CONTROL:
		*ret = -1;
		return 0;
	case _SC_SAVED_IDS:
	case _SC_VERSION:
		*ret = 200809;
		return 0;
	case _SC_LINE_MAX:
		*ret = 4096;
		return 0;
	case _SC_RE_DUP_MAX:
		*ret = 32767;
		return 0;
	case _SC_LOGIN_NAME_MAX:
	case _SC_TTY_NAME_MAX:
		*ret = 32;
		return 0;
	case _SC_GETPW_R_SIZE_MAX:
	case _SC_GETGR_R_SIZE_MAX:
		*ret = 1024;
		return 0;
	case _SC_ATEXIT_MAX:
		*ret = 32;
		return 0;
	case _SC_DELAYTIMER_MAX:
		*ret = 32;
		return 0;
	case _SC_SEM_NSEMS_MAX:
		*ret = 256;
		return 0;
	case _SC_TIMER_MAX:
		*ret = 64;
		return 0;
	default:
		return EINVAL;
	}
}

int Sysdeps<Umask>::operator()(mode_t mode, mode_t *old) {
	if (old) *old = umask_value;
	umask_value = mode;
	return 0;
}

void Sysdeps<Sync>::operator()() {
	syscall(SYS_SYNC);
}

int Sysdeps<Fdatasync>::operator()(int fd) {
	auto ret = syscall(SYS_FDATASYNC, fd);
	if (sc_error(ret))
		return sc_error(ret);
	return 0;
}

int Sysdeps<Fsync>::operator()(int fd) {
	auto ret = syscall(SYS_FSYNC, fd);
	if (sc_error(ret))
		return sc_error(ret);
	return 0;
}

int Sysdeps<Utimensat>::operator()(int dirfd, const char *pathname,
		const struct timespec times[2], int flags) {
	struct timespec now;
	NyonosTimespec2 tv;

	clock_gettime(CLOCK_REALTIME, &now);

	if (!times) {
		tv.asec = now.tv_sec;
		tv.msec = now.tv_sec;
	} else {
		tv.asec = times[0].tv_nsec == UTIME_NOW ? now.tv_sec : times[0].tv_sec;
		tv.msec = times[1].tv_nsec == UTIME_NOW ? now.tv_sec : times[1].tv_sec;
	}

	auto ret = syscall(SYS_UTIMENSAT, dirfd, reinterpret_cast<__sc_word_t>(pathname),
			reinterpret_cast<__sc_word_t>(&tv), flags);
	if (sc_error(ret))
		return sc_error(ret);
	return 0;
}

int Sysdeps<Ftruncate>::operator()(int fd, size_t size) {
	auto ret = syscall(SYS_FTRUNCATE, fd, size);
	if (sc_error(ret))
		return sc_error(ret);
	return 0;
}

int Sysdeps<Mkdir>::operator()(const char *path, mode_t mode) {
	auto ret = syscall(SYS_MKDIR, reinterpret_cast<__sc_word_t>(path), mode);
	if (sc_error(ret))
		return sc_error(ret);
	return 0;
}

int Sysdeps<Mkdirat>::operator()(int dirfd, const char *path, mode_t mode) {
	auto ret = syscall(SYS_MKDIRAT, dirfd, reinterpret_cast<__sc_word_t>(path), mode);
	if (sc_error(ret))
		return sc_error(ret);
	return 0;
}

int Sysdeps<Unlinkat>::operator()(int dirfd, const char *path, int flags) {
	auto ret = syscall(SYS_UNLINKAT, dirfd, reinterpret_cast<__sc_word_t>(path), flags);
	if (sc_error(ret))
		return sc_error(ret);
	return 0;
}

int Sysdeps<Rmdir>::operator()(const char *path) {
	auto ret = syscall(SYS_RMDIR, reinterpret_cast<__sc_word_t>(path));
	if (sc_error(ret))
		return sc_error(ret);
	return 0;
}

int Sysdeps<Truncate>::operator()(const char *path, off_t length) {
	auto ret = syscall(SYS_TRUNCATE, reinterpret_cast<__sc_word_t>(path), length);
	if (sc_error(ret))
		return sc_error(ret);
	return 0;
}

int Sysdeps<Flock>::operator()(int fd, int options) { (void)fd; (void)options; return 0; }

int Sysdeps<Fadvise>::operator()(int fd, off_t offset, off_t length, int advice) {
	(void)fd; (void)offset; (void)length; (void)advice;
	return 0;
}

int Sysdeps<Madvise>::operator()(void *addr, size_t length, int advice) {
	(void)addr; (void)length; (void)advice;
	return 0;
}

int Sysdeps<PosixMadvise>::operator()(void *addr, size_t length, int advice) {
	(void)addr; (void)length; (void)advice;
	return 0;
}

int Sysdeps<Mlock>::operator()(const void *addr, size_t length) { (void)addr; (void)length; return 0; }
int Sysdeps<Munlock>::operator()(const void *addr, size_t length) { (void)addr; (void)length; return 0; }
int Sysdeps<Mlockall>::operator()(int flags) { (void)flags; return 0; }
int Sysdeps<Munlockall>::operator()() { return 0; }

int Sysdeps<GetAffinity>::operator()(pid_t pid, size_t cpusetsize, cpu_set_t *mask) {
	(void)pid;
	if (cpusetsize < sizeof(cpu_set_t)) return EINVAL;
	__builtin_memset(mask, 0, sizeof(cpu_set_t));
	((unsigned char *)mask)[0] = 1;
	return 0;
}

int Sysdeps<SetAffinity>::operator()(pid_t pid, size_t cpusetsize, const cpu_set_t *mask) {
	(void)pid; (void)cpusetsize; (void)mask;
	return 0;
}

int Sysdeps<GetThreadaffinity>::operator()(pid_t tid, size_t cpusetsize, cpu_set_t *mask) {
	return sysdep<GetAffinity>(tid, cpusetsize, mask);
}

int Sysdeps<SetThreadaffinity>::operator()(pid_t tid, size_t cpusetsize, const cpu_set_t *mask) {
	return sysdep<SetAffinity>(tid, cpusetsize, mask);
}

int Sysdeps<GetScheduler>::operator()(pid_t pid, int *policy) {
	(void)pid;
	*policy = SCHED_OTHER;
	return 0;
}

int Sysdeps<SetScheduler>::operator()(pid_t pid, int policy, const struct sched_param *param) {
	(void)pid; (void)policy; (void)param;
	return 0;
}

int Sysdeps<GetParam>::operator()(pid_t pid, struct sched_param *param) {
	(void)pid;
	param->sched_priority = 0;
	return 0;
}

int Sysdeps<SetParam>::operator()(pid_t pid, const struct sched_param *param) {
	(void)pid; (void)param;
	return 0;
}

int Sysdeps<GetPriority>::operator()(int which, id_t who, int *value) {
	(void)which; (void)who;
	*value = 0;
	return 0;
}

int Sysdeps<SetPriority>::operator()(int which, id_t who, int prio) {
	(void)which; (void)who; (void)prio;
	return 0;
}

int Sysdeps<GetMinPriority>::operator()(int policy, int *out) {
	(void)policy;
	*out = 0;
	return 0;
}

int Sysdeps<GetMaxPriority>::operator()(int policy, int *out) {
	*out = (policy == SCHED_RR || policy == SCHED_FIFO) ? 99 : 0;
	return 0;
}

int Sysdeps<GetRlimit>::operator()(int resource, struct rlimit *limit) {
	switch (resource) {
	case RLIMIT_NOFILE:
		limit->rlim_cur = 256;
		limit->rlim_max = 256;
		break;
	case RLIMIT_NPROC:
		limit->rlim_cur = 64;
		limit->rlim_max = 64;
		break;
	default:
		limit->rlim_cur = RLIM_INFINITY;
		limit->rlim_max = RLIM_INFINITY;
		break;
	}
	return 0;
}

int Sysdeps<SetRlimit>::operator()(int resource, const struct rlimit *limit) {
	if (resource == RLIMIT_NOFILE && limit->rlim_cur > 256) return EPERM;
	if (resource == RLIMIT_NPROC && limit->rlim_cur > 64) return EPERM;
	return 0;
}

int Sysdeps<Times>::operator()(struct tms *tms, clock_t *out) {
	time_t s = 0;
	long ns = 0;
	if (int e = sysdep<ClockGet>(0, &s, &ns)) return e;

	tms->tms_utime = 0;
	tms->tms_stime = 0;
	tms->tms_cutime = 0;
	tms->tms_cstime = 0;
	*out = (clock_t)(s / 10);
	return 0;
}

}
