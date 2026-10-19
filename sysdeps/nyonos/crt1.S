.section .text
.global _start
.type _start, @function
_start:
	xor %rbp, %rbp

	mov %rsp, %rdi
	lea main(%rip), %rsi
	call __mlibc_entry

	hlt
	.size _start, . - _start

.section .note.GNU-stack,"",@progbits
