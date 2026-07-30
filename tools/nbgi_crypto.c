// nbgi_crypto.c — Blowfish shim
#include "nbgi_types.h"
#include <string.h>
#include <openssl/blowfish.h>

static BF_KEY g_bf_key;

void BlowfishSetKey(LPBYTE lpKey, int bits) {
    BF_set_key(&g_bf_key, bits / 8, lpKey);
}

void BlowfishDecrypt(LPDWORD pLeft, LPDWORD pRight) {
    uint8_t block[8];
    uint32_t L = _byteswap_ulong(*pLeft);
    uint32_t R = _byteswap_ulong(*pRight);
    memcpy(block,     &L, 4);
    memcpy(block + 4, &R, 4);
    BF_ecb_encrypt(block, block, &g_bf_key, BF_DECRYPT);
    memcpy(&L, block,     4);
    memcpy(&R, block + 4, 4);
    *pLeft  = _byteswap_ulong(L);
    *pRight = _byteswap_ulong(R);
}

void BlowfishEncrypt(LPDWORD pLeft, LPDWORD pRight) {
    uint8_t block[8];
    uint32_t L = _byteswap_ulong(*pLeft);
    uint32_t R = _byteswap_ulong(*pRight);
    memcpy(block,     &L, 4);
    memcpy(block + 4, &R, 4);
    BF_ecb_encrypt(block, block, &g_bf_key, BF_ENCRYPT);
    memcpy(&L, block,     4);
    memcpy(&R, block + 4, 4);
    *pLeft  = _byteswap_ulong(L);
    *pRight = _byteswap_ulong(R);
}
