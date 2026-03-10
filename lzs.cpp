#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#endif

typedef unsigned char u8;

static int EI = 12;
static int EJ = 4;
static int P = 2;
static int rless = 2;
static int init_chr = 0;

static void lzss_set_window(unsigned char* window, int window_size, int init_chr) {
    memset(window, init_chr, window_size);
}

int unlzss(unsigned char* src, int srclen, unsigned char* dst, int dstlen) {
    static int slide_winsz = 0;
    static unsigned char* slide_win = NULL;
    unsigned char* dststart = dst;
    unsigned char* srcend = src + srclen;
    unsigned char* dstend = dst + dstlen;
    int i, j, k, r, c;
    unsigned flags;

    int N = 1 << EI;
    int F = 1 << EJ;

    if (N > slide_winsz) {
        unsigned char* new_win = (unsigned char*)realloc(slide_win, N);
        if (new_win) {
            slide_win = new_win;
            slide_winsz = N;
        }
        else {
            return -1;
        }
    }
    lzss_set_window(slide_win, N, init_chr);

    r = (N - F) - rless;
    N--;
    F--;

    for (flags = 0;; flags >>= 1) {
        if (!(flags & 0x100)) {
            if (src >= srcend) break;
            flags = *src++;
            flags |= 0xff00;
        }
        if (flags & 1) {
            if (src >= srcend) break;
            c = *src++;
            if (dst >= dstend) return -1;
            *dst++ = c;
            slide_win[r] = c;
            r = (r + 1) & N;
        }
        else {
            if (src >= srcend) break;
            i = *src++;
            if (src >= srcend) break;
            j = *src++;
            i |= ((j >> EJ) << 8);
            j = (j & F) + P;
            for (k = 0; k <= j; k++) {
                c = slide_win[(i + k) & N];
                if (dst >= dstend) return -1;
                *dst++ = c;
                slide_win[r] = c;
                r = (r + 1) & N;
            }
        }
    }
    return (int)(dst - dststart);
}

int decompress_file(const char* input_file) {
    FILE* in = fopen(input_file, "rb");
    if (!in) {
        printf("无法打开输入文件: %s\n", input_file);
        return -1;
    }

    fseek(in, 0, SEEK_END);
    long file_size = ftell(in);
    fseek(in, 0, SEEK_SET);

    if (file_size < 4) {
        printf("文件太小\n");
        fclose(in);
        return -1;
    }

    unsigned char* compressed_data = (unsigned char*)malloc(file_size);
    if (!compressed_data) {
        printf("内存分配失败\n");
        fclose(in);
        return -1;
    }

    fread(compressed_data, 1, file_size, in);
    fclose(in);

    uint32_t uncompressed_size = 0;
    uncompressed_size |= compressed_data[0];
    uncompressed_size |= compressed_data[1] << 8;
    uncompressed_size |= compressed_data[2] << 16;
    uncompressed_size |= compressed_data[3] << 24;

    int compressed_size = (int)(file_size - 4);

    unsigned char* uncompressed_data = (unsigned char*)malloc(uncompressed_size);
    if (!uncompressed_data) {
        printf("解压缓冲区分配失败\n");
        free(compressed_data);
        return -1;
    }

    int result = unlzss(compressed_data + 4, compressed_size, uncompressed_data, uncompressed_size);

    if (result < 0) {
        printf("解压失败\n");
        free(compressed_data);
        free(uncompressed_data);
        return -1;
    }

    char output_file[1024];
    strcpy(output_file, input_file);
    char* dot = strrchr(output_file, '.');
    if (dot) {
        strcpy(dot, ".dec");
    }
    else {
        strcat(output_file, ".dec");
    }

    FILE* out = fopen(output_file, "wb");
    if (!out) {
        printf("无法创建输出文件\n");
        free(compressed_data);
        free(uncompressed_data);
        return -1;
    }

    fwrite(uncompressed_data, 1, result, out);
    fclose(out);

    printf("解压成功: %s -> %s (%d 字节 -> %d 字节)\n", input_file, output_file, compressed_size, result);

    free(compressed_data);
    free(uncompressed_data);
    return 0;
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        printf("用法: %s <文件路径>\n", argv[0]);
        printf("遍历指定路径下所有.lzs文件进行解压\n");
        return -1;
    }

#ifdef _WIN32
    char search_path[1024];
    sprintf(search_path, "%s\\*.lzs", argv[1]);

    WIN32_FIND_DATAA file_info;
    HANDLE handle = FindFirstFileA(search_path, &file_info);

    if (handle == INVALID_HANDLE_VALUE) {
        printf("未找到.lzs文件\n");
        return -1;
    }

    int success_count = 0;
    int total_count = 0;

    do {
        char file_path[1024];
        sprintf(file_path, "%s\\%s", argv[1], file_info.cFileName);

        printf("\n处理文件: %s\n", file_info.cFileName);
        if (decompress_file(file_path) == 0) {
            success_count++;
        }
        total_count++;

    } while (FindNextFileA(handle, &file_info) != 0);

    FindClose(handle);
#else
    DIR* dir = opendir(argv[1]);
    if (!dir) {
        printf("无法打开路径: %s\n", argv[1]);
        return -1;
    }

    int success_count = 0;
    int total_count = 0;
    struct dirent* entry;

    while ((entry = readdir(dir)) != NULL) {
        if (strstr(entry->d_name, ".lzs") != NULL) {
            char file_path[1024];
            sprintf(file_path, "%s/%s", argv[1], entry->d_name);

            printf("\n处理文件: %s\n", entry->d_name);
            if (decompress_file(file_path) == 0) {
                success_count++;
            }
            total_count++;
        }
    }

    closedir(dir);
#endif

    printf("\n处理完成: %d/%d 个文件成功解压\n", success_count, total_count);
    return 0;
}