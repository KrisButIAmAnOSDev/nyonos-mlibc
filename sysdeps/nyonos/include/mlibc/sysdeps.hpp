#pragma once

#include <mlibc/sysdep-signatures.hpp>

namespace mlibc {

struct NyonosSysdepTags :
	AnonAllocate,
	AnonFree,
	ClockGet,
	Close,
	Exit,
	FutexWait,
	FutexWake,
	Isatty,
	LibcLog,
	LibcPanic,
	Open,
	Pread,
	Read,
	Seek,
	Stat,
	TcbSet,
	VmMap,
	VmUnmap,
	Write
{};

template<typename Tag>
using Sysdeps = SysdepOf<NyonosSysdepTags, Tag>;

struct SysdepTraits {
	static constexpr bool usesRtNetlink = false;
};

}
