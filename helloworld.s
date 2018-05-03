# helloworld.s
.text
.global main
.type main, @function

main:
  pushq %rax       # 现场保存，先入栈，后出栈
  pushq %rbx
  pushq %rcx
  pushq %rdx

  movq $8, %rax    # 系统调用号(sys_create)  创建文件
  movq $content, %rbx
  mov $00644,%rcx
  int $0x80

  movq %rax, %rbx
  movq $4, %rax    # 系统调用号(sys_write)  写入文件
  movq $content, %rcx
  movq $10, %rdx   # 10为字符串的大小
  int $0x80

  movq $6,%rax    
  int $0x80

  popq %rdx
  popq %rcx
  popq %rbx
  popq %rax

  movq $1,%rax   # 以下三行将被替换为跳转指令
  mov $0, %rbx
  int $0x80
content:
  .string "helloworld"
