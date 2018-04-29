.text
.global main
.type main, @function

main:
  pushq %rax
  pushq %rbx
  pushq %rcx
  pushq %rdx

  movq $8, %rax
  movq $.L, %rbx
  mov $00644,%rcx
  int $0x80

  movq %rax, %rbx
  movq $4, %rax
  movq $.L, %rcx
  movq $5,%rdx

  int $0x80

  movq $6,%rax
  int $0x80

  popq %rdx
  popq %rcx
  popq %rbx
  popq %rax

  movq $1,%rax
  mov $0, %rbx
  int $0x80
.L:
  .string "hhhhh"
