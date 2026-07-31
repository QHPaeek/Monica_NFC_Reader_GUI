/*
 * accesscode.cpp — Amusement IC SPAD 解密 + 卡号转换
 * 从 Monica_F072_NFC 固件移植
 */
#include "accesscode.h"
#include "sbox.h"
#include <cstring>

bool decrypt_accesscode(uint8_t spad[16])
{
    // 第一步: S_BOX_INV[8] 逆掩码, 覆盖全部 16 字节
    for (int i = 0; i < 16; i++)
        spad[i] = S_BOX_INV[N_TABLES][spad[i]];

    // 第二步: 从参数字节提取迭代参数
    uint8_t count = (spad[15] >> 4) + 7;
    uint8_t table = spad[15] + ITER_ADD * count;

    // 第三步: 逆序迭代(解密方向)
    for (uint8_t iter = 0; iter < count; iter++) {
        table -= ITER_ADD;

        // rotate_right 前 15 字节, 5 位
        {
            uint8_t prior = spad[14];
            for (int i = 0; i < 15; i++) {
                uint8_t temp = spad[i];
                spad[i] = (spad[i] >> 5) | ((prior & 0x1F) << 3);
                prior = temp;
            }
        }

        // S_BOX_INV[table % N_TABLES] 替换前 15 字节
        for (int i = 0; i < 15; i++)
            spad[i] = S_BOX_INV[table % N_TABLES][spad[i]];
    }

    // 前 6 字节全零 → 解密有效
    return (spad[0] | spad[1] | spad[2] | spad[3] | spad[4] | spad[5]) == 0;
}

void ascii_to_accesscode(const uint8_t *ascii_data, uint8_t *accesscode)
{
    accesscode[0] = 0x00;
    accesscode[1] = 0x00;
    for (int i = 0; i < 8; i++)
        accesscode[i + 2] = (ascii_data[2*i] - 0x30) << 4 | (ascii_data[2*i + 1] - 0x30);
}

void hex_to_accesscode(const uint8_t *hex, uint8_t *accesscode)
{
    accesscode[0] = 0x00;
    accesscode[1] = 0x00;
    memcpy(accesscode + 2, hex, 8);
    for (int i = 0; i < 8; i++) {
        uint8_t hi = (hex[i] >> 4);
        uint8_t lo = (hex[i] & 0x0F);
        if (hi >= 10) hi -= 7;
        if (lo >= 10) lo -= 7;
        accesscode[i + 2] = (hi << 4) | lo;
    }
}

void idm_to_accesscode(const uint8_t uid[8], uint8_t out[16])
{
    uint64_t val = 0;
    for (int i = 0; i < 8; i++)
        val = (val << 8) | uid[i];
    for (int i = 0; i < 6; i++)
        out[i] = 0x00;
    for (int i = 0; i < 10; i++) {
        uint8_t lo = (uint8_t)(val % 10);
        val /= 10;
        uint8_t hi = (uint8_t)(val % 10);
        val /= 10;
        out[15 - i] = (uint8_t)((hi << 4) | lo);
    }
}
