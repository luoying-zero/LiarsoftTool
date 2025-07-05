// RScriptText.cpp
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <iconv.h>
#include <string>

// 替换Windows类型定义
typedef uint32_t DWORD;
typedef unsigned char BYTE;
typedef uint32_t ULONG;
typedef void* PVOID;
#define CP_UTF8 65001

// 修改结构体对齐方式
typedef struct RScriptHeader {
    DWORD FileSize;
    DWORD HeaderSize;
    DWORD ByteCodeSize1;
    DWORD ByteCodeSize2;
    DWORD StringPoolSize;
    DWORD ByteCodeSize3;
    DWORD ByteCodeSize4;
} __attribute__((packed)) RScriptHeader;

// 编码转换函数
std::string convert_encoding(const std::string& source, const char* from, const char* to) {
    iconv_t cd = iconv_open(to, from);
    if (cd == (iconv_t)-1) {
        return "";
    }

    size_t inbytesleft = source.size();
    size_t outbytesleft = inbytesleft * 4; // 保守估计输出缓冲区大小
    std::string result(outbytesleft, '\0');
    char* inbuf = const_cast<char*>(source.data());
    char* outbuf = &result[0];
    char* outptr = outbuf;

    if (iconv(cd, &inbuf, &inbytesleft, &outptr, &outbytesleft) == -1) {
        iconv_close(cd);
        return "";
    }

    result.resize(outptr - outbuf);
    iconv_close(cd);
    return result;
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        return 0;
    }

    FILE* fin = fopen(argv[1], "rb");
    if (!fin) {
        return 0;
    }

    // 获取文件大小
    fseek(fin, 0, SEEK_END);
    long Size = ftell(fin);
    rewind(fin);

    BYTE* Buffer = new BYTE[Size];
    if (!Buffer) {
        fclose(fin);
        return 0;
    }

    fread(Buffer, 1, Size, fin);
    fclose(fin);

    RScriptHeader* Header = (RScriptHeader*)Buffer;
    if (Header->StringPoolSize == 0) {
        delete[] Buffer;
        return 0;
    }

    ULONG PostOffset = Header->HeaderSize + Header->ByteCodeSize1 + Header->ByteCodeSize2;
    ULONG iPos = 0;

    std::string OutName = argv[1];
    OutName += ".txt";

    FILE* fout = fopen(OutName.c_str(), "wb");

    while (iPos < Header->StringPoolSize) {
        char* strStart = (char*)(Buffer + PostOffset + iPos);
        std::string sjis_str(strStart);

        // 转换Shift-JIS到UTF-8
        std::string utf8_str = convert_encoding(sjis_str, "SHIFT_JIS", "UTF-8");
        if (utf8_str.empty()) {
            utf8_str = sjis_str; // 转换失败时使用原始字符串
        }

        // 写入输出文件
        fprintf(fout, "[0x%08x]%s\r\n", PostOffset + iPos, utf8_str.c_str());
        fprintf(fout, ";[0x%08x]%s\r\n", PostOffset + iPos, utf8_str.c_str());
        fprintf(fout, ">[0x%08x]\r\n\r\n", PostOffset + iPos);

        // 移动到下一个字符串
        iPos += strlen(strStart) + 1;
    }

    fclose(fout);
    delete[] Buffer;
    return 0;
}
