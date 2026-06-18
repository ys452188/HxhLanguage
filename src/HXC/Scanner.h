#ifndef HXHLANG_SRC_HXC_SCANNER_H
#define HXHLANG_SRC_HXC_SCANNER_H
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

int readSourceFile(FILE* fp, wchar_t** src) noexcept;
/*
  将 UTF-8 字节序列解码为wchar_t
  - ptr:指向UTF-8数据，len为字节长度
  - 输出: *out为null结尾的wchar_t*（由函数 malloc 出来，调用者负责
  free），返回 0 成功，非 0 失败
*/
static int UTF8ToWStr(const char* ptr, size_t len, wchar_t** out) noexcept {
    if (!ptr || !out) return -1;
    *out = NULL;
    size_t i = 0;
    size_t needed = 0;
    while (i < len) {
        unsigned char c = (unsigned char)ptr[i];
        if (c < 0x80) {
            needed += 1;
            i += 1;
        } else if ((c >> 5) == 0x6) {
            // 2-byte
            if (i + 1 >= len) return -1;
            needed += 1;
            i += 2;
        } else if ((c >> 4) == 0xE) {
            // 3-byte
            if (i + 2 >= len) return -1;
            needed += 1;
            i += 3;
        } else if ((c >> 3) == 0x1E) {
            if (i + 3 >= len) return -1;
            if (sizeof(wchar_t) == 2)
                needed += 2;
            else
                needed += 1;
            i += 4;
        } else {
            return -1;
        }
    }
    wchar_t* wbuf = (wchar_t*)malloc((needed + 1) * sizeof(wchar_t));
    if (!wbuf) return -1;

    i = 0;
    size_t wi = 0;
    while (i < len) {
        uint32_t codepoint = 0;
        unsigned char c = (unsigned char)ptr[i];
        if (c < 0x80) {
            codepoint = c;
            i += 1;
        } else if ((c >> 5) == 0x6) {
            if (i + 1 >= len) {
                free(wbuf);
                return -1;
            }
            unsigned char c1 = (unsigned char)ptr[i + 1];
            if ((c1 >> 6) != 0x2) {
                free(wbuf);
                return -1;
            }
            codepoint = ((c & 0x1F) << 6) | (c1 & 0x3F);
            i += 2;
        } else if ((c >> 4) == 0xE) {
            if (i + 2 >= len) {
                free(wbuf);
                return -1;
            }
            unsigned char c1 = (unsigned char)ptr[i + 1];
            unsigned char c2 = (unsigned char)ptr[i + 2];
            if ((c1 >> 6) != 0x2 || (c2 >> 6) != 0x2) {
                free(wbuf);
                return -1;
            }
            codepoint = ((c & 0x0F) << 12) | ((c1 & 0x3F) << 6) | (c2 & 0x3F);
            i += 3;
        } else if ((c >> 3) == 0x1E) {
            if (i + 3 >= len) {
                free(wbuf);
                return -1;
            }
            unsigned char c1 = (unsigned char)ptr[i + 1];
            unsigned char c2 = (unsigned char)ptr[i + 2];
            unsigned char c3 = (unsigned char)ptr[i + 3];
            if ((c1 >> 6) != 0x2 || (c2 >> 6) != 0x2 || (c3 >> 6) != 0x2) {
                free(wbuf);
                return -1;
            }
            codepoint = ((c & 0x07) << 18) | ((c1 & 0x3F) << 12) | ((c2 & 0x3F) << 6) | (c3 & 0x3F);
            i += 4;
        } else {
            free(wbuf);
            return -1;
        }

        if (sizeof(wchar_t) == 2) {
            if (codepoint <= 0xFFFF) {
                wbuf[wi++] = (wchar_t)codepoint;
            } else if (codepoint <= 0x10FFFF) {
                codepoint -= 0x10000;
                wchar_t high = (wchar_t)((codepoint >> 10) + 0xD800);
                wchar_t low = (wchar_t)((codepoint & 0x3FF) + 0xDC00);
                wbuf[wi++] = high;
                wbuf[wi++] = low;
            } else {
                free(wbuf);
                return -1;
            }
        } else {
            // wchar_t >= 4
            wbuf[wi++] = (wchar_t)codepoint;
        }
    }

    wbuf[wi] = (wchar_t)0;
    *out = wbuf;
    return 0;
}

/*
  readSourceFile_no_windows:
  - path: 文件路径，使用 C 字符串打开 (fopen, "rb")。
  - src: 输出的 wchar_t* ，由函数分配，调用者负责 free。
  返回 0 成功，非 0 失败。
*/
int readSourceFile(FILE* fp, wchar_t** src) noexcept {
    if (!fp || !src) return -1;
    *src = NULL;

    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return -1;
    }
    long lsize = ftell(fp);
    if (lsize < 0) {
        fclose(fp);
        return -1;
    }
    size_t size = (size_t)lsize;
    rewind(fp);

    char* buf = (char*)malloc(size + 1);
    if (!buf) {
        fclose(fp);
        return -1;
    }

    size_t r = fread(buf, 1, size, fp);
    fclose(fp);
    if (r != size) {
        free(buf);
        return -1;
    }
    buf[size] = '\0';

    size_t offset = 0;
    if (size >= 3 && (unsigned char)buf[0] == 0xEF && (unsigned char)buf[1] == 0xBB && (unsigned char)buf[2] == 0xBF) {
        // UTF-8 BOM
        offset = 3;
        int rc = UTF8ToWStr(buf + offset, size - offset, src);
        free(buf);
        return rc;
    } else if (size >= 2 && (unsigned char)buf[0] == 0xFF && (unsigned char)buf[1] == 0xFE) {
        // UTF-16 LE BOM
        size_t bytes = size - 2;
        if (bytes % 2 != 0) {
            free(buf);
            return -1;
        }
        size_t wcnt = bytes / 2;
        wchar_t* out = (wchar_t*)malloc((wcnt + 1) * sizeof(wchar_t));
        if (!out) {
            free(buf);
            return -1;
        }
        // 在小端平台 (Windows) 直接 memcpy 即可
        memcpy(out, buf + 2, bytes);
        out[wcnt] = L'\0';
        free(buf);
        *src = out;
        return 0;
    } else if (size >= 2 && (unsigned char)buf[0] == 0xFE && (unsigned char)buf[1] == 0xFF) {
        // UTF-16 BE BOM: 需要字节交换后复制
        size_t bytes = size - 2;
        if (bytes % 2 != 0) {
            free(buf);
            return -1;
        }
        size_t wcnt = bytes / 2;
        wchar_t* out = (wchar_t*)malloc((wcnt + 1) * sizeof(wchar_t));
        if (!out) {
            free(buf);
            return -1;
        }
        // 逐字交换字节对
        for (size_t i = 0; i < wcnt; ++i) {
            unsigned char hi = (unsigned char)buf[2 + i * 2];
            unsigned char lo = (unsigned char)buf[2 + i * 2 + 1];
            uint16_t v = (uint16_t)((hi << 8) | lo);
            uint16_t v_le = (uint16_t)((v >> 8) | (v << 8));
            out[i] = (wchar_t)v_le;
        }
        out[wcnt] = L'\0';
        free(buf);
        *src = out;
        return 0;
    } else {
        // 无BOM
        int rc = UTF8ToWStr(buf, size, src);
        free(buf);
        return rc;
    }
}

#endif