#include <stdio.h>
#include <unistd.h>
#include <elf.h>
#include <fcntl.h>
#include <stdlib.h>
//暂定为4096，即默认被插入的代码大小 < 4096
#define pageSize 4096

Elf64_Addr program_head_vaddr;
Elf64_Xword program_head_size;
Elf64_Off entry_section_offset;
Elf64_Xword entry_section_size;

void workOutAddr(int value, int arr[])
{
    int a = value / (16 * 16);
    int b = value % (16 * 16);
    int a2 = a / (16 * 16);
    int b2 = a % (16 * 16);

    arr[0] = b;
    arr[1] = b2;
    arr[2] = a2;
}

void insert(Elf64_Addr new_entry, Elf64_Addr orig_entry, int origfile, int dirSecTail)
{
    //需要修改跳转指令中的目的地址为原入口地址
    int ori_arr[3];
    workOutAddr(orig_entry, ori_arr);

    //计算出数据的地址,扔到将插入的代码中
    int dataEntry = new_entry + 73;
    int data_arr[3];
    workOutAddr(dataEntry, data_arr);

    //机器码。作用是创建一个文件写入字符串
    //其中0x5*是将压栈与弹栈保护其中原始内容
    char binary[] = {
        0x50,
        0x53,
        0x51,
        0x52,
        0x48, 0xc7, 0xc0, 0x08, 0x00, 0x00, 0x00,
        0x48, 0xc7, 0xc3, data_arr[0], data_arr[1], data_arr[2], 0x00,
        0x48, 0xc7, 0xc1, 0xa4, 0x01, 0x00, 0x00,
        0xcd, 0x80,
        0x48, 0x89, 0xc3,
        0x48, 0xc7, 0xc0, 0x04, 0x00, 0x00, 0x00,
        0x48, 0xc7, 0xc1, data_arr[0], data_arr[1], data_arr[2], 0x00,
        0x48, 0xc7, 0xc2, 0x05, 0x00, 0x00, 0x00,
        0xcd, 0x80,
        0x48, 0xc7, 0xc0, 0x06, 0x00, 0x00, 0x00,
        0xcd, 0x80,
        0x5a,
        0x59,
        0x5b,
        0x58,

        //跳转指令
        0xbd, ori_arr[0], ori_arr[1], ori_arr[2], 0x00, 0xff, 0xe5,

        //数据区域
        0x68, 0x68, 0x68, 0x68, 0x68, 0x00

    };

    //将机器码插入
    printf("插入代码\n");
    lseek(origfile, dirSecTail, 0);
    write(origfile, binary, sizeof(binary));

    //对齐
    printf("对齐代码\n");
    char tmp[pageSize - sizeof(binary)] = {0};
    write(origfile, tmp, pageSize - sizeof(binary));
}

int main(int argc, char **argv)
{
    /**
   * 读取ELF文件头信息
   */
    //存储器
    char elf_ehdr[sizeof(Elf64_Ehdr)];
    Elf64_Ehdr *p_ehdr = (Elf64_Ehdr *)elf_ehdr;

    printf("打开文件并读取ELF头信息\n");
    int origfile = open(argv[1], O_RDWR);
    read(origfile, elf_ehdr, sizeof(elf_ehdr));
    //保存原入口地址
    Elf64_Addr orig_entry = p_ehdr->e_entry;

    /**
   * 读取并修改程序头
   */
    //存储器
    char elf_phdr[sizeof(Elf64_Phdr)];
    Elf64_Phdr *p_phdr = (Elf64_Phdr *)elf_phdr;
    int perProHeadSize = sizeof(elf_phdr);

    int flag = 0, i;
    int programHeadNum = (int)p_ehdr->e_phnum;
    for (i = 0; i < programHeadNum; i++)
    {
        read(origfile, elf_phdr, sizeof(Elf64_Phdr));
        if (flag == 0 && p_phdr->p_vaddr <= orig_entry && (p_phdr->p_vaddr + p_phdr->p_filesz) > orig_entry)
        {
            //找到程序段入口
            printf("找到了目标程序段\n");
            flag = 1;
            program_head_vaddr = p_phdr->p_vaddr;
            program_head_size = p_phdr->p_filesz;

            p_phdr->p_filesz += pageSize;
            p_phdr->p_memsz += pageSize;

            lseek(origfile, -perProHeadSize, 1);
            write(origfile, elf_phdr, perProHeadSize);
        }
        else if (flag != 0)
        {
            p_phdr->p_offset += pageSize;
            lseek(origfile, -perProHeadSize, 1);
            write(origfile, elf_phdr, perProHeadSize);
        }
    }

    /**
   * 读取并修改节头
   */
    //将节头向下移
    //要先获得 节区的数量 以及 单个节区头部的大小
    int sectionNum = (int)p_ehdr->e_shnum;
    int perSecHeadSize = p_ehdr->e_shentsize;
    int secHeadSize = sectionNum * perSecHeadSize;
    //存储器
    char elf_shdr[sizeof(Elf64_Shdr)];
    Elf64_Shdr *p_shdr = (Elf64_Shdr *)elf_shdr;

    int s_i;
    int dirSecHeadOff;
    Elf64_Addr new_entry;
    for (i = sectionNum - 1; i >= 0; i--)
    {
        lseek(origfile, p_ehdr->e_shoff + i * perSecHeadSize, 0);
        read(origfile, elf_shdr, sizeof(elf_shdr));
        if (p_shdr->sh_addr + p_shdr->sh_size == program_head_vaddr + program_head_size)
        {
            entry_section_offset = p_shdr->sh_offset;
            entry_section_size = p_shdr->sh_size;
            new_entry = p_shdr->sh_addr + p_shdr->sh_size;
            s_i = i;

            dirSecHeadOff = p_ehdr->e_shoff + i * sizeof(elf_shdr);
            printf("找到了目标节区的节区头的偏移：%04x\n", dirSecHeadOff);
            p_shdr->sh_size += pageSize;
        }
        else
        {
            //改其offset值
            p_shdr->sh_offset += pageSize;
        }

        lseek(origfile, pageSize - sizeof(elf_shdr), 1);
        write(origfile, elf_shdr, sizeof(elf_shdr));

        if (s_i != 0)
        {
            break;
        }
    }

    int dirSecTail = entry_section_offset + entry_section_size;
    printf("找到了目标节区的末尾：%04x\n", dirSecTail);
    int dirSecTail_dirSecHeadOff = dirSecHeadOff - dirSecTail;
    printf("找到了从 目标节区末尾 到目标节区头 的长度：%04x\n", dirSecTail_dirSecHeadOff);
    //这段内容保存着从 目标节区末尾 到目标节区头 的内容
    char con[dirSecTail_dirSecHeadOff];
    lseek(origfile, dirSecTail, 0);
    read(origfile, con, dirSecTail_dirSecHeadOff);

    //将指针移到目标节区尾部，写入这一大段内容
    printf("将这一段下移\n");
    lseek(origfile, dirSecTail + pageSize, 0);
    write(origfile, con, dirSecTail_dirSecHeadOff);
    insert(new_entry, orig_entry, origfile, dirSecTail);

    /**
   * 修改ELF头
   */
    printf("修改ELF头\n");
    p_ehdr->e_entry = new_entry;
    p_ehdr->e_shoff += pageSize;
    lseek(origfile, 0, 0);
    write(origfile, elf_ehdr, sizeof(elf_ehdr));

    close(origfile);
}
