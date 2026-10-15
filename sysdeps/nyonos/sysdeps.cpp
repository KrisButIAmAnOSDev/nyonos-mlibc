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
}

int Sysdeps<FutexWake>::operator()(int *pointer, bool all) {
	auto ret = syscall(SYS_FUTEX, reinterpret_cast<__sc_word_t>(pointer), 1u, 0, 0);
	return sc_error(ret);
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

}
