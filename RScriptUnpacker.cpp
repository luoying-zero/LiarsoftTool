// RScriptUnpacker.cpp
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// 类型定义
typedef uint32_t DWORD;
typedef unsigned char BYTE;
typedef uint32_t ULONG;

// 结构体定义（使用 packed 对齐）
typedef struct Header {
    DWORD Magic;
    DWORD ChunkSize;
    DWORD ChunkCount;
} __attribute__((packed)) Header;

typedef struct ChunkItem {
    char FileName[0x20];
    DWORD Offset;
    DWORD Size;
} __attribute__((packed)) ChunkItem;

int main(int argc, char* argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <input_file>\n", argv[0]);
        return 1;
    }

    FILE* fin = fopen(argv[1], "rb");
    if (!fin) {
        perror("Failed to open input file");
        return 1;
    }

    Header FileHeader;
    if (fread(&FileHeader, 1, sizeof(FileHeader), fin) != sizeof(FileHeader)) {
        fprintf(stderr, "Failed to read header\n");
        fclose(fin);
        return 1;
    }

    if (FileHeader.Magic != 0x0001424cUL) {
        fprintf(stderr, "Invalid magic number\n");
        fclose(fin);
        return 1;
    }

    BYTE* ChunkData = new BYTE[FileHeader.ChunkSize];
    if (!ChunkData) {
        fprintf(stderr, "Memory allocation failed\n");
        fclose(fin);
        return 1;
    }

    if (fread(ChunkData, 1, FileHeader.ChunkSize, fin) != FileHeader.ChunkSize) {
        fprintf(stderr, "Failed to read chunk data\n");
        delete[] ChunkData;
        fclose(fin);
        return 1;
    }

    ULONG PostOffset = sizeof(FileHeader) + FileHeader.ChunkSize;

    for (ULONG i = 0; i < FileHeader.ChunkCount; i++) {
        ChunkItem* item = (ChunkItem*)(ChunkData + i * sizeof(ChunkItem));

        // 确保文件名以空字符结尾
        char null_terminated_name[0x20];
        memcpy(null_terminated_name, item->FileName, 0x20);
        null_terminated_name[0x1f] = '\0';

        // 移动文件指针到数据位置
        if (fseek(fin, PostOffset + item->Offset, SEEK_SET) != 0) {
            fprintf(stderr, "Failed to seek in file\n");
            continue;
        }

        FILE* fout = fopen(null_terminated_name, "wb");
        if (!fout) {
            fprintf(stderr, "Failed to create output file: %s\n", null_terminated_name);
            continue;
        }

        BYTE* FileData = new BYTE[item->Size];
        if (!FileData) {
            fprintf(stderr, "Memory allocation failed for file data\n");
            fclose(fout);
            continue;
        }

        if (fread(FileData, 1, item->Size, fin) != item->Size) {
            fprintf(stderr, "Failed to read file data\n");
            delete[] FileData;
            fclose(fout);
            continue;
        }

        if (fwrite(FileData, 1, item->Size, fout) != item->Size) {
            fprintf(stderr, "Failed to write file data\n");
        }

        delete[] FileData;
        fclose(fout);
    }

    delete[] ChunkData;
    fclose(fin);
    return 0;
}