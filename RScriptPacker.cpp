#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <vector>
#include <string>

// 替换 Windows 类型定义
typedef uint32_t DWORD;
typedef unsigned char BYTE;
typedef uint32_t ULONG;

// 文件头结构体（使用 packed 对齐）
typedef struct Header {
    DWORD Magic;          // 固定为 0x0001424c
    DWORD ChunkSize;      // ChunkItem 数组总大小
    DWORD ChunkCount;     // 文件数量
} __attribute__((packed)) Header;

// 文件项结构体（使用 packed 对齐）
typedef struct ChunkItem {
    char FileName[0x20];  // 文件名（需以 \0 结尾）
    DWORD Offset;         // 文件数据相对于 PostOffset 的偏移
    DWORD Size;           // 文件大小
} __attribute__((packed)) ChunkItem;

// 获取目录下的所有文件列表
std::vector<std::string> get_files_in_dir(const char* dir_path) {
    std::vector<std::string> files;
    DIR* dir = opendir(dir_path);
    if (!dir) {
        perror("Failed to open directory");
        return files;
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (entry->d_type == DT_REG) { // 普通文件
            files.push_back(entry->d_name);
        }
    }
    closedir(dir);
    return files;
}

// 读取文件内容到内存
BYTE* read_file(const char* path, size_t* out_size) {
    FILE* fp = fopen(path, "rb");
    if (!fp) return nullptr;

    fseek(fp, 0, SEEK_END);
    *out_size = ftell(fp);
    rewind(fp);

    BYTE* buffer = new BYTE[*out_size];
    if (!buffer) {
        fclose(fp);
        return nullptr;
    }

    if (fread(buffer, 1, *out_size, fp) != *out_size) {
        delete[] buffer;
        fclose(fp);
        return nullptr;
    }

    fclose(fp);
    return buffer;
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <input_dir> <output_file>\n", argv[0]);
        return 1;
    }

    const char* input_dir = argv[1];
    const char* output_file = argv[2];

    // 获取文件列表
    std::vector<std::string> files = get_files_in_dir(input_dir);
    if (files.empty()) {
        fprintf(stderr, "No files found in directory\n");
        return 1;
    }

    ULONG chunk_count = files.size();
    ULONG chunk_size = chunk_count * sizeof(ChunkItem);
    ULONG total_data_size = 0;

    // 计算总数据大小
    for (const auto& file : files) {
        std::string full_path = std::string(input_dir) + "/" + file;
        FILE* fp = fopen(full_path.c_str(), "rb");
        if (!fp) continue;

        fseek(fp, 0, SEEK_END);
        total_data_size += ftell(fp);
        fclose(fp);
    }

    // 分配内存
    Header header = {
        .Magic = 0x0001424c,
        .ChunkSize = chunk_size,
        .ChunkCount = chunk_count
    };

    BYTE* chunk_data = new BYTE[chunk_size];
    BYTE* file_data = new BYTE[total_data_size];

    if (!chunk_data || !file_data) {
        fprintf(stderr, "Memory allocation failed\n");
        delete[] chunk_data;
        delete[] file_data;
        return 1;
    }

    // 填充 ChunkItem 数据
    ULONG current_offset = 0;
    BYTE* file_data_ptr = file_data;
    for (ULONG i = 0; i < chunk_count; ++i) {
        ChunkItem* item = (ChunkItem*)(chunk_data + i * sizeof(ChunkItem));
        const std::string& file_name = files[i];
        std::string full_path = std::string(input_dir) + "/" + file_name;

        // 填充文件名
        strncpy(item->FileName, file_name.c_str(), 0x1f);
        item->FileName[0x1f] = '\0';

        // 读取文件内容
        size_t file_size;
        BYTE* file_content = read_file(full_path.c_str(), &file_size);
        if (!file_content) {
            fprintf(stderr, "Failed to read file: %s\n", full_path.c_str());
            continue;
        }

        // 填充偏移和大小
        item->Offset = current_offset;
        item->Size = file_size;

        // 复制文件数据
        memcpy(file_data_ptr, file_content, file_size);
        current_offset += file_size;
        file_data_ptr += file_size;
        delete[] file_content;
    }

    // 写入输出文件
    FILE* fout = fopen(output_file, "wb");
    if (!fout) {
        fprintf(stderr, "Failed to create output file\n");
        delete[] chunk_data;
        delete[] file_data;
        return 1;
    }

    // 写入头部
    fwrite(&header, 1, sizeof(header), fout);

    // 写入 ChunkItem 数据
    fwrite(chunk_data, 1, chunk_size, fout);

    // 写入文件数据
    fwrite(file_data, 1, total_data_size, fout);

    fclose(fout);
    delete[] chunk_data;
    delete[] file_data;

    printf("Successfully packed %u files into %s\n", chunk_count, output_file);
    return 0;
}
