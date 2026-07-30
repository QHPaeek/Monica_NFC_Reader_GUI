// nbgi_types.h — type aliases, avoids Windows.h dependency
#ifndef NBGI_TYPES_H
#define NBGI_TYPES_H
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

typedef uint8_t   BYTE;
typedef uint16_t  WORD;
typedef uint32_t  DWORD;
// UINT64 — 联合体: 支持标量(=0)和 b[8] 字节 (BE 序, b[0]=MSB)
typedef union {
    uint64_t q;
    uint8_t  b[8];
} UINT64;
typedef uint32_t  UINT32;
typedef uint16_t  UINT16;
typedef uint8_t   UINT8;
typedef char      CHAR;

typedef uint8_t  *LPBYTE;
typedef uint16_t *LPWORD;
typedef uint32_t *LPDWORD;
typedef void     *LPVOID;

// Windows-like helpers
#define _byteswap_ulong(x) __builtin_bswap32(x)
#define _countof(a) (sizeof(a) / sizeof((a)[0]))

static inline void ZeroMemory(void *p, size_t n) { memset(p, 0, n); }

#endif
