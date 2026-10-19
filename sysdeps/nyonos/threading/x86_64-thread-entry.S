.section .text
.global __mlibc_thread_entry
.type __mlibc_thread_entry, @function
__mlibc_thread_entry:
	pop %rdi
	pop %rsi
	pop %rdx
	call __mlibc_thread_trampoline
	hlt
	.size __mlibc_thread_entry, . - __mlibc_thread_entry

.section .note.GNU-stack,"",@progbits