#include "nbgi_uint64.h"
#include <string.h>

// UINT64 被定义为 union, 同时支持标量运算和逐字节访问
// 字节始终按大端序排列: b[0]=MSB, b[7]=LSB
// 这与原始代码的行为一致

bool UINT64IsZero(UINT64 *pU64) {
    return pU64->q == 0;
}

UINT64 UINT64Add(UINT64 *pU64_1, UINT64 *pU16_2) {
    UINT64 res;
    uint32_t carry = 0;
    // 逐字节 BE 加法: b[7]=LSB, b[0]=MSB
    for (int i = 7; i >= 0; i--) {
        carry += pU64_1->b[i] + pU16_2->b[i];
        res.b[i] = carry & 0xFF;
        carry >>= 8;
    }
    return res;
}

UINT32 UINT64ModUInt32(UINT64 *pU64, UINT32 u32) {
    uint32_t res = 0;
    for (int i = 0; i < 8; i++)   // b[0]=MSB → 从左到右处理
        res = ((res << 8) + pU64->b[i]) % u32;
    return res;
}

UINT64 UINT64DivideUInt32(UINT64 *pU64, UINT32 u32) {
    UINT64 res;
    uint32_t carry = 0;
    for (int i = 0; i < 8; i++) {
        res.b[i] = ((carry << 8) + pU64->b[i]) / u32;
        carry    = ((carry << 8) + pU64->b[i]) % u32;
    }
    return res;
}

void UINT64ToA(char *lpStr, UINT64 *pU64) {
    char buf[64], *tail = buf;
    int i = 0;
    UINT64 tmp = *pU64;
    while (!UINT64IsZero(&tmp)) {
        *tail++ = (char)(UINT64ModUInt32(&tmp, 10) + '0');
        tmp = UINT64DivideUInt32(&tmp, 10);
        i++;
    }
    for (; i < 17; i++) *lpStr++ = '0';
    while (tail != buf) *lpStr++ = *--tail;
}

UINT64 IDDataToUINT64(LPBYTE lpIdData) {
    // temp 数组是大端序字节: lpIdData[0]=MSB
    UINT64 res;
    memcpy(res.b, lpIdData, 8);
    return res;
}
