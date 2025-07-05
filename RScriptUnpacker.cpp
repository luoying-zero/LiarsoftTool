//RScriptUnpacker
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <libgen.h>
#include <string>     // ✅ 新增：定义 std::string
#include <vector>

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

// ✅ 新增：字符串工具函数
bool create_directory(const char* dir_path) {
    struct stat st = {0};
    if (stat(dir_path, &st) == -1) {
        #ifdef _WIN32
        if (mkdir(dir_path) != 0)
        #else
        if (mkdir(dir_path, 0755) != 0)
        #endif
        {
            perror("Failed to create directory");
            return false;
        }
    }
    return true;
}

// ✅ 新增：去除文件扩展名
std::string remove_extension(const std::string& filename) {
    size_t last_dot = filename.find_last_of(".");
    if (last_dot == std::string::npos) return filename;
    return filename.substr(0, last_dot);
}

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

    // ✅ 使用 std::string
    std::string input_path = argv[1];
    std::string input_dir = dirname((char*)input_path.c_str());
    std::string input_name = basename((char*)input_path.c_str());

    std::string output_dir = input_dir + "/" + remove_extension(input_name);
    printf("Creating directory: %s\n", output_dir.c_str());
    if (!create_directory(output_dir.c_str())) {
        fclose(fin);
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

        char null_terminated_name[0x20];
        memcpy(null_terminated_name, item->FileName, 0x20);
        null_terminated_name[0x1f] = '\0';

        std::string output_path = output_dir + "/" + null_terminated_name;

        if (fseek(fin, PostOffset + item->Offset, SEEK_SET) != 0) {
            fprintf(stderr, "Failed to seek in file\n");
            continue;
        }

        FILE* fout = fopen(output_path.c_str(), "wb");
        if (!fout) {
            fprintf(stderr, "Failed to create output file: %s\n", output_path.c_str());
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
    printf("Successfully unpacked to directory: %s\n", output_dir.c_str());
    return 0;
}