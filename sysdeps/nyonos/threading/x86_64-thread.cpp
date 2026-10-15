#include <sys/mman.h>
#include <bits/syscall.h>
#include <bits/nyonos-syscall.h>
#include <mlibc/debug.hpp>
#include <mlibc/all-sysdeps.hpp>
#include <bits/ensure.h>
#include <mlibc/tcb.hpp>
#include <errno.h>

extern "C" [[gnu::visibility("hidden")]] void __mlibc_thread_entry();

static int sc_error(long ret) {
	if (ret < 0 && ret >= -4095)
		return static_cast<int>(-ret);
	return 0;
}

extern "C" void __mlibc_thread_trampoline(void *(*fn)(void *), Tcb *tcb, void *arg) {
	if(mlibc::sysdep<TcbSet>(tcb))
		__ensure(!"failed to set tcb for new thread");

	while(__atomic_load_n(&tcb->tid, __ATOMIC_RELAXED) == 0)
		mlibc::sysdep<FutexWait>(&tcb->tid, 0, nullptr);

	mlibc::thread_exit_return ret;
	if(tcb->returnValueType == TcbThreadReturnValue::Integer)
		ret.integer = reinterpret_cast<int (*)(void *)>(fn)(arg);
	else
		ret.voidPtr = fn(arg);

	mlibc::thread_exit(ret);
}

namespace mlibc {

int Sysdeps<PrepareStack>::operator()(
		void **stack,
		void *entry,
		void *arg,
		void *tcb,
		size_t *stack_size,
		size_t *guard_size,
		void **stack_base) {
	if(!stack || !entry || !tcb || !stack_size || !stack_base)
		return EINVAL;

	*guard_size = 0;

	size_t want = *stack_size;
	if(want < 16384)
		want = 16384;
	*stack_size = want;

	void *base;
	if(*stack) {
		base = *stack;
	} else {
		long ret = syscall(SYS_MMAP, (__sc_word_t)want, 3, ABI_MMAP_ANON);
		if(ret < 0)
			return static_cast<int>(-ret);
		base = reinterpret_cast<void *>(static_cast<__sc_word_t>(ret));
	}

	*stack_base = base;

	void **it = reinterpret_cast<void **>(reinterpret_cast<char *>(base) + want);
	*--it = arg;
	*--it = tcb;
	*--it = entry;

	*stack = reinterpret_cast<void *>(it);
	return 0;
}

int Sysdeps<Clone>::operator()(void *tcb, pid_t *pid_out, void *stack) {
	if(!tcb || !pid_out || !stack)
		return EINVAL;

	long ret = syscall(SYS_NEWTHREAD, reinterpret_cast<__sc_word_t>(pid_out),
			reinterpret_cast<__sc_word_t>(__mlibc_thread_entry),
			reinterpret_cast<__sc_word_t>(stack));
	return sc_error(ret);
}

void Sysdeps<ThreadExit>::operator()() {
	syscall(SYS_THREAD_EXIT);
	__builtin_unreachable();
}

pid_t Sysdeps<GetTid>::operator()() {
	long ret = syscall(SYS_GETPID);
	if(sc_error(ret))
		return ret;
	return static_cast<pid_t>(ret);
}

} // namespace mlibc