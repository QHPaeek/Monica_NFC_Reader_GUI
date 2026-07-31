/*
 * accesscode.h — Amusement IC SPAD 解密 + 卡号转换
 * 从 Monica_F072_NFC 固件移植
 */
#pragma once
#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

// 解密 AIC SPAD0 数据, 原地修改
// 返回: true=解密有效(前6字节全零), false=需要走 IDM 转换
bool decrypt_accesscode(uint8_t spad[16]);

void ascii_to_accesscode(const uint8_t *ascii_data, uint8_t *accesscode);
void hex_to_accesscode(const uint8_t *hex, uint8_t *accesscode);
void idm_to_accesscode(const uint8_t uid[8], uint8_t out[16]);

#ifdef __cplusplus
}
#endif
