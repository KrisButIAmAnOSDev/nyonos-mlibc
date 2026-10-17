#pragma once

#include <mlibc/sysdep-signatures.hpp>

namespace mlibc {

struct NyonosSysdepTags :
	AnonAllocate,
	AnonFree,
	Clone,
	ClockGet,
	Close,
	Dup,
	Waitpid,
	Sleep,
	Openat,
	Fork,
	Dup2,
	GetTid,
	Exit,
	Fcntl,
	FutexWait,
	FutexWake,
	GetPid,
	Ioctl,
	Isatty,
	LibcLog,
	LibcPanic,
	Open,
	OpenDir,
	PrepareStack,
	Pread,
	Read,
	ReadEntries,
	Seek,
	Stat,
	ThreadExit,
	TcbSet,
	VmMap,
	VmUnmap,
	Write,
	Yield
{};

template<typename Tag>
using Sysdeps = SysdepOf<NyonosSysdepTags, Tag>;

struct SysdepTraits {
	static constexpr bool usesRtNetlink = false;
};

}
