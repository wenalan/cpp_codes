.global _start
.intel_syntax noprefix

_start:
	mov rax, 1
	mov rdi, 1
	lea rsi, [hello]
	mov rdx, 14
	syscall

	mov rax, 60
	mov rdi, 42
	syscall

hello:
	.asciz "Hello, World!\n"
