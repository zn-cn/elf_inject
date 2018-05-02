# elf 文件注入

操作系统大作业

## 课题描述：

在 Linux 中修改一个现有的 elf 可执行程序（显然没有源代码！譬如 vi 或其他如何自己编写的可执行程序）。让该程序运行后先执行一个特别的附件功能（附加功能是：创建或者打开一个指定文件写入 helloworld 的字符串）后再继续运行该程序，否则程序结束。注意：附加功能嵌入到原来的程序中，不是一个独立的程序！要求了解 elf 文件格式，另外建议使用汇编编程。

## 操作流程

#### 测试用例介绍：

本elf注入的功能是在elf文件执行前，生成helloworld文件并写入内容：helloworld，之后再执行原elf文件功能

```
gcc main.c -o main  # 生成注入函数
# 测试文件功能：打印 This is the program, whick will be injected.\n
gcc test.c -o test  # 生成测试elf文件
./main test   # elf注入
./test  # 执行注入后的elf文件
```

#### main.c中的二进制码的生成过程如下：

+ 首先书写汇编代码：

  功能：生成helloworld文件并写入内容：helloworld

  参考：

  + [IBM Linux汇编语言开发指南](https://www.ibm.com/developerworks/cn/linux/l-assembly/index.html) 
  + [汇编学习-使用文件](https://my.oschina.net/u/2537915/blog/700886) 
  + [linux 系统调用号表](https://blog.csdn.net/qq_29343201/article/details/52209588) 

  注：此处为64位环境，采用 AT&T 格式，若为32位环境，请改用eax, ebx 等

  ```asm
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

    movq $1,%rax
    mov $0, %rbx
    int $0x80
  content:
    .string "helloworld"
  ```

+ 编译为 .o 二进制文件

  ```
  gcc -c helloworld.s # 默认生成 helloworld.o
  ```

+ 反编译

  ```
  objdump -s -d hhhhh.o > hhhhh.o.txt
  ```

  文件：helloworld.o.txt

  ```assembly

  helloworld.o：     文件格式 elf64-x86-64

  Contents of section .text:
   0000 50535152 48c7c008 00000048 c7c30000  PSQRH......H....
   0010 000048c7 c1a40100 00cd8048 89c348c7  ..H........H..H.
   0020 c0040000 0048c7c1 00000000 48c7c20a  .....H......H...
   0030 000000cd 8048c7c0 06000000 cd805a59  .....H........ZY
   0040 5b5848c7 c0010000 0048c7c3 00000000  [XH......H......
   0050 cd806865 6c6c6f77 6f726c64 00        ..helloworld.   

  Disassembly of section .text:

  0000000000000000 <main>:
     0:	50                   	push   %rax
     1:	53                   	push   %rbx
     2:	51                   	push   %rcx
     3:	52                   	push   %rdx
     4:	48 c7 c0 08 00 00 00 	mov    $0x8,%rax
     b:	48 c7 c3 00 00 00 00 	mov    $0x0,%rbx
    12:	48 c7 c1 a4 01 00 00 	mov    $0x1a4,%rcx
    19:	cd 80                	int    $0x80
    1b:	48 89 c3             	mov    %rax,%rbx
    1e:	48 c7 c0 04 00 00 00 	mov    $0x4,%rax
    25:	48 c7 c1 00 00 00 00 	mov    $0x0,%rcx
    2c:	48 c7 c2 0a 00 00 00 	mov    $0xa,%rdx
    33:	cd 80                	int    $0x80
    35:	48 c7 c0 06 00 00 00 	mov    $0x6,%rax
    3c:	cd 80                	int    $0x80
    3e:	5a                   	pop    %rdx
    3f:	59                   	pop    %rcx
    40:	5b                   	pop    %rbx
    41:	58                   	pop    %rax
    42:	48 c7 c0 01 00 00 00 	mov    $0x1,%rax
    49:	48 c7 c3 00 00 00 00 	mov    $0x0,%rbx
    50:	cd 80                	int    $0x80

  0000000000000052 <content>:
    52:	68 65 6c 6c 6f       	pushq  $0x6f6c6c65
    57:	77 6f                	ja     c8 <content+0x76>
    59:	72 6c                	jb     c7 <content+0x75>
    5b:	64                   	fs
  	...
  ```

+ 选取其中十六进制的数字作为 main.c 中变量 inject_data

  注意注释

  ```
      char inject_data[] = {
          0x50,
          0x53,
          0x51,
          0x52,
          0x48, 0xc7, 0xc0, 0x08, 0x00, 0x00, 0x00,
          // 由于注入之后数据段的地址修改了，故此处需要自行计算新的地址，具体请参见代码
          0x48, 0xc7, 0xc3, new_addr[0], new_addr[1], new_addr[2], 0x00,
          0x48, 0xc7, 0xc1, 0xa4, 0x01, 0x00, 0x00,
          0xcd, 0x80,
          0x48, 0x89, 0xc3,
          0x48, 0xc7, 0xc0, 0x04, 0x00, 0x00, 0x00,
          // 由于注入之后数据段的地址修改了，故此处需要自行计算新的地址，具体请参见代码
          0x48, 0xc7, 0xc1, new_addr[0], new_addr[1], new_addr[2], 0x00,
          0x48, 0xc7, 0xc2, 0x0a, 0x00, 0x00, 0x00,
          0xcd, 0x80,
          0x48, 0xc7, 0xc0, 0x06, 0x00, 0x00, 0x00,
          0xcd, 0x80,
          0x5a,
          0x59,
          0x5b,
          0x58,
          //跳转指令
          // 此处在原来的汇编程序中为程序中断指令，修改为跳转到原入口地址elfh.e_entry 
          0xbd, ori_addr[0], ori_addr[1], ori_addr[2], 0x00, 0xff, 0xe5,
          
          //数据区域 helloworld
          0x68, 0x65, 0x6c, 0x6c, 0x6f,
          0x77, 0x6f,
          0x72, 0x6c,
          0x64,
          0x00
      };
  ```

  ​

## ELF初体验

elf 目标文件有三种类型:

- 可重定位文件(Relocatable File) .o)包含适合于与其他目标文件链接来创建可执行文件或者共享目标文件的代码和数据。
- 可执行文件(Executable File) .exe) 包含适合于执行的一个程序，此文件规定了exec() 如何创建一个程序的进程映像。
- 共享目标文件(Shared Object File) .so) 包含可在两种上下文中链接的代码和数据。首先链接编辑器可以将它和其它可重定位文件和共享目标文件一起处理， 生成另外一个目标文件。其次动态链接器(Dynamic Linker)可能将它与某 个可执行文件以及其它共享目标一起组合，创建进程映像。

目标文件全部是程序的二进制表示，目的是直接在某种处理器上直接执行。

## ELF文件格式

### 目标文件格式

目标文件既要参与程序链接又要参与程序执行。出于方便性和效率考虑，目标文件
格式提供了两种并行视图，分别反映了这些活动的不同需求。

![目标文件格式](http://gnaixx.cc/blog_images/elf/1.png)

文件开始处是一个 ELF 头部(ELF Header)，用来描述整个文件的组织。节区部 分包含链接视图的大量信息:指令、数据、符号表、重定位信息等等。

程序头部表(Program Header Table)，如果存在的话，告诉系统如何创建进程映像。 用来构造进程映像的目标文件必须具有程序头部表，可重定位文件不需要这个表。

节区头部表(Section Heade Table)包含了描述文件节区的信息，每个节区在表中 都有一项，每一项给出诸如节区名称、节区大小这类信息。用于链接的目标文件必须包 含节区头部表，其他目标文件可以有，也可以没有这个表。

**注意：** 尽管图中显示的各个组成部分是有顺序的，实际上除了 ELF 头部表以外， 其他节区和段都没有规定的顺序

### 目标文件中数据表示

目标文件格式支持 8 位字节/32 位体系结构。不过这种格式是可以扩展的，比如现在的64位机器，目标文件因此以某些机器独立的格式表达某些控制数据，使得能够以一种公共的方式来识别和解释其内容。目标文件中的其它数据使用目标处理器的编码结构，而不管文件在何种机器上创建。

![img](http://gnaixx.cc/blog_images/elf/2.png)

目标文件中的所有数据结构都遵从“自然”大小和对齐规则。如果必要，数据结构可 以包含显式的补齐，例如为了确保 4 字节对象按 4 字节边界对齐。数据对齐同样适用于文件内部。

### ELF Hearder部分

文件的最开始几个字节给出如何解释文件的提示信息。这些信息独立于处理器，也
独立于文件中的其余内容。ELF Header 部分可以用以下的数据结构表示:

```
/* ELF Header */
typedef struct elfhdr {
    unsigned char    e_ident[EI_NIDENT]; /* ELF Identification */
    Elf32_Half    e_type;        /* object file type */
    Elf32_Half    e_machine;    /* machine */
    Elf32_Word    e_version;    /* object file version */
    Elf32_Addr    e_entry;    /* virtual entry point */
    Elf32_Off    e_phoff;    /* program header table offset */
    Elf32_Off    e_shoff;    /* section header table offset */
    Elf32_Word    e_flags;    /* processor-specific flags */
    Elf32_Half    e_ehsize;    /* ELF header size */
    Elf32_Half    e_phentsize;    /* program header entry size */
    Elf32_Half    e_phnum;    /* number of program header entries */
    Elf32_Half    e_shentsize;    /* section header entry size */
    Elf32_Half    e_shnum;    /* number of section header entries */
    Elf32_Half    e_shstrndx;    /* section header table's "section 
                       header string table" entry offset */
} Elf32_Ehdr;

typedef struct {
    unsigned char    e_ident[EI_NIDENT];    /* Id bytes */
    Elf64_Quarter    e_type;            /* file type */
    Elf64_Quarter    e_machine;        /* machine type */
    Elf64_Half    e_version;        /* version number */
    Elf64_Addr    e_entry;        /* entry point */
    Elf64_Off    e_phoff;        /* Program hdr offset */
    Elf64_Off    e_shoff;        /* Section hdr offset */
    Elf64_Half    e_flags;        /* Processor flags */
    Elf64_Quarter    e_ehsize;        /* sizeof ehdr */
    Elf64_Quarter    e_phentsize;        /* Program header entry size */
    Elf64_Quarter    e_phnum;        /* Number of program headers */
    Elf64_Quarter    e_shentsize;        /* Section header entry size */
    Elf64_Quarter    e_shnum;        /* Number of section headers */
    Elf64_Quarter    e_shstrndx;        /* String table index */
} Elf64_Ehdr;
```

其中，e_ident 数组给出了 ELF 的一些标识信息，这个数组中不同下标的含义如表所示:

![e_ident标识索引](http://gnaixx.cc/blog_images/elf/3.png)

这些索引访问包含以下数值的字节:

![e_ident的内容说明](http://gnaixx.cc/blog_images/elf/4.png)

e_ident[EI_MAG0]~e_ident[EI_MAG3]即e_ident[0]~e_ident[3]被称为魔数（Magic Number）,其值一般为0x7f,'E','L','F'。 
e_ident[EI_CLASS]（即e_ident[4]）识别目标文件运行在目标机器的类别，取值可为三种值：ELFCLASSNONE（0）非法类别；ELFCLASS32（1）32位目标；ELFCLASS64（2）64位目标。 
e_ident[EI_DATA]（即e_ident[5]）：给出处理器特定数据的数据编码方式。即大端还是小端方式。取值可为3种：ELFDATANONE（0）非法数据编码；ELFDATA2LSB（1）高位在前；ELFDATA2MSB（2）低位在前。

ELF Header 中各个字段的说明如表：

![ELF Header中各个字段的含义](http://gnaixx.cc/blog_images/elf/5.png)

一个实际可执行文件的头文件头部形式如下：

```
$greadelf -h hello.so
ELF 头：
  Magic：  7f 45 4c 46 01 01 01 00 00 00 00 00 00 00 00 00
  类别:                             ELF32
  数据:                             2 补码，小端序 (little endian)
  版本:                             1 (current)
  OS/ABI:                           UNIX - System V
  ABI 版本:                         0
  类型:                             DYN (共享目标文件)
  系统架构:                          ARM
  版本:                             0x1
  入口点地址：                       0x0
  程序头起点：                       52 (bytes into file)
  Start of section headers:        61816 (bytes into file)
  标志：                            0x5000000, Version5 EABI
  本头的大小：                       52 (字节)
  程序头大小：                       32 (字节)
  Number of program headers:       9
  节头大小：                        40 (字节)
  节头数量：                        24
  字符串表索引节头：                 23
```

**注：**linux环境下可以利用命令readelf,mac需要安装binutils。用brew安装很方便，命令：`brew update && brew install binutils`

### 程序头部（Program Header）

可执行文件或者共享目标文件的程序头部是一个结构数组，每个结构描述了一个段 或者系统准备程序执行所必需的其它信息。目标文件的“段”包含一个或者多个“节区”， 也就是“段内容(Segment Contents)”。程序头部仅对于可执行文件和共享目标文件 有意义。 
可执行目标文件在 ELF 头部的 e_phentsize 和 e_phnum 成员中给出其自身程序头部 的大小。程序头部的数据结构:

```
/* Program Header */
typedef struct {
    Elf32_Word    p_type;        /* segment type */
    Elf32_Off    p_offset;    /* segment offset */
    Elf32_Addr    p_vaddr;    /* virtual address of segment */
    Elf32_Addr    p_paddr;    /* physical address - ignored? */
    Elf32_Word    p_filesz;    /* number of bytes in file for seg. */
    Elf32_Word    p_memsz;    /* number of bytes in mem. for seg. */
    Elf32_Word    p_flags;    /* flags */
    Elf32_Word    p_align;    /* memory alignment */
} Elf32_Phdr;

typedef struct {
    Elf64_Half    p_type;        /* entry type */
    Elf64_Half    p_flags;    /* flags */
    Elf64_Off    p_offset;    /* offset */
    Elf64_Addr    p_vaddr;    /* virtual address */
    Elf64_Addr    p_paddr;    /* physical address */
    Elf64_Xword    p_filesz;    /* file size */
    Elf64_Xword    p_memsz;    /* memory size */
    Elf64_Xword    p_align;    /* memory & file alignment */
} Elf64_Phdr;
```

其中各个字段说明：

- p_type 此数组元素描述的段的类型，或者如何解释此数组元素的信息。具体如下图。
- p_offset 此成员给出从文件头到该段第一个字节的偏移。
- p_vaddr 此成员给出段的第一个字节将被放到内存中的虚拟地址。
- p_paddr 此成员仅用于与物理地址相关的系统中。因为 System V 忽略所有应用程序的物理地址信息，此字段对与可执行文件和共享目标文件而言具体内容是指定的。
- p_filesz 此成员给出段在文件映像中所占的字节数。可以为 0。
- p_memsz 此成员给出段在内存映像中占用的字节数。可以为 0。
- p_flags 此成员给出与段相关的标志。
- p_align 可加载的进程段的 p_vaddr 和 p_offset 取值必须合适，相对于对页面大小的取模而言。此成员给出段在文件中和内存中如何 对齐。数值 0 和 1 表示不需要对齐。否则 p_align 应该是个正整数，并且是 2 的幂次数，p_vaddr 和 p_offset 对 p_align 取模后应该相等。

可执行 ELF 目标文件中的段类型：

![段类型](http://gnaixx.cc/blog_images/elf/6.png)

### 节区（Sections）

节区中包含目标文件中的所有信息，除了:ELF 头部、程序头部表格、节区头部
表格。节区满足以下条件:

1. 目标文件中的每个节区都有对应的节区头部描述它，反过来，有节区头部不意 味着有节区。
2. 每个节区占用文件中一个连续字节区域(这个区域可能长度为 0)。
3. 文件中的节区不能重叠，不允许一个字节存在于两个节区中的情况发生。
4. 目标文件中可能包含非活动空间(INACTIVE SPACE)。这些区域不属于任何头部和节区，其内容指定。

#### 节区头部表格

ELF 头部中，e_shoff 成员给出从文件头到节区头部表格的偏移字节数;e_shnum 给出表格中条目数目;e_shentsize 给出每个项目的字节数。从这些信息中可以确切地定 位节区的具体位置、长度。

每个节区头部数据结构描述:

```
/* Section Header */
typedef struct {
    Elf32_Word    sh_name;    /* name - index into section header
                       string table section */
    Elf32_Word    sh_type;    /* type */
    Elf32_Word    sh_flags;    /* flags */
    Elf32_Addr    sh_addr;    /* address */
    Elf32_Off    sh_offset;    /* file offset */
    Elf32_Word    sh_size;    /* section size */
    Elf32_Word    sh_link;    /* section header table index link */
    Elf32_Word    sh_info;    /* extra information */
    Elf32_Word    sh_addralign;    /* address alignment */
    Elf32_Word    sh_entsize;    /* section entry size */
} Elf32_Shdr;

typedef struct {
    Elf64_Half    sh_name;    /* section name */
    Elf64_Half    sh_type;    /* section type */
    Elf64_Xword    sh_flags;    /* section flags */
    Elf64_Addr    sh_addr;    /* virtual address */
    Elf64_Off    sh_offset;    /* file offset */
    Elf64_Xword    sh_size;    /* section size */
    Elf64_Half    sh_link;    /* link to another */
    Elf64_Half    sh_info;    /* misc info */
    Elf64_Xword    sh_addralign;    /* memory alignment */
    Elf64_Xword    sh_entsize;    /* table entry size */
} Elf64_Shdr;
```

对其中各个字段的解释如下：

![节区头部字段说明](http://gnaixx.cc/blog_images/elf/7.png)

索引为零（SHN_UNDEF）的节区头部是存在的，尽管此索引标记的是未定义的节区应用。这个节区固定为：

![SHN_UNDEF(0)节区内容](http://gnaixx.cc/blog_images/elf/8.png)

##### sh_type 字段

节区类型定义：

![节区类型定义](https://static.segmentfault.com/v-5ae2d994/global/img/squares.svg)

其他的节区类型是保留的。

##### sh_flags 字段

sh_flags 字段定义了一个节区中包含的内容是否可以修改、是否可以执行等信息。 如果一个标志位被设置，则该位取值为 1。 定义的各位都设置为 0。

![sh_flags 字段取值](http://gnaixx.cc/blog_images/elf/10.png)

其中已经定义了的各位含义如下:

- SHF_WRITE: 节区包含进程执行过程中将可写的数据。
- SHF_ALLOC: 此节区在进程执行过程中占用内存。某些控制节区并不出现于目标
  文件的内存映像中，对于那些节区，此位应设置为 0。
- SHF_EXECINSTR: 节区包含可执行的机器指令。
- SHF_MASKPROC: 所有包含于此掩码中的四位都用于处理器专用的语义。

##### sh_link 和 sh_info 字段

根据节区类型的不同，sh_link 和 sh_info 的具体含义也有所不同:
![sh_link 和 sh_info 字段解释](http://gnaixx.cc/blog_images/elf/11.png)

#### 特殊节区

很多节区中包含了程序和控制信息。下面的表中给出了系统使用的节区，以及它们 的类型和属性。

![常见特殊节区](https://static.segmentfault.com/v-5ae2d994/global/img/squares.svg)

在分析这些节区的时候，需要注意如下事项:

- 以“.”开头的节区名称是系统保留的。应用程序可以使用没有前缀的节区名称，以避 免与系统节区冲突。
- 目标文件格式允许人们定义不在上述列表中的节区。
- 目标文件中也可以包含多个名字相同的节区。
- 保留给处理器体系结构的节区名称一般构成为:处理器体系结构名称简写 + 节区
  名称。
- 处理器名称应该与 e_machine 中使用的名称相同。例如 .FOO.psect 街区是由
  FOO 体系结构定义的 psect 节区。

另外，有些编译器对如上节区进行了扩展，这些已存在的扩展都使用约定俗成的名
称，如:

- .sdata
- .tdesc
- .sbss
- .lit4
- .lit8
- .reginfo
- .gptab
- .liblist
- .conflict
- ...

## 思路

1. (也可以最后修改)修正 ELF 头部 中的 e_shoff ，增加 PAGESIZE 大小（操作系统页式系统，一页默认4k）
2. 修正 ELF 头部 中的 e_entry ，指向 p_vaddr + p_filesz
3. 修正 第一个程序头部表，第一个头部特殊对待，因为要插入自己注入的程序，所以要把第一个头部扩容，把p_filesz 和 p_memsz 增加 PAGESIZE 大小或者 注入程序的大小
4. 修正程序头部表偏移地址p_offset ，增加 PAGESIZE 大小
5. 修正节区 sh_offset ，增加 PAGESIZE 大小
6. 修正注入程序机器码，如：修正数据段存储地址（从新的elfh.e_entry开始计算程序首地址），最后要加上跳转指令，即跳转到原来的e_entry（刚开始记录的）
7. 插入修正后的注入程序的机器码
8. 插入 0 ，让插入区块扩充到 PAGESIZE （4k）