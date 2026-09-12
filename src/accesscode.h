/*
 * accesscode.h — Amusement IC SPAD 解密 + 卡号转换
 * 从 Monica_F072_NFC 固件移植
 */
#pragma once
#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

// 解密 AIC SPAD0 数据, 原地修改 (与固件 accesscode.c 保持一致)
//
// 解密完成后 spad[6..15] 即为 20 位 BCD 卡号 (每字节 2 位十进制数字)。
// 若 spad[6] 不在 0x50..0x59 (即卡号首位不是 5) 则认为解密结果不可用,
// 此时清零前 6 字节并用 idm 重新生成卡号填入 spad[6..15]。
//
// 注意: SPAD 前 6 字节疑似会放其他数据, 原"前 6 字节全零"的判据已废弃。
void decrypt_accesscode(uint8_t spad[16], const uint8_t idm[8]);

void ascii_to_accesscode(const uint8_t *ascii_data, uint8_t *accesscode);
void hex_to_accesscode(const uint8_t *hex, uint8_t *accesscode);
void idm_to_accesscode(const uint8_t uid[8], uint8_t out[16]);

#ifdef __cplusplus
}
#endif
