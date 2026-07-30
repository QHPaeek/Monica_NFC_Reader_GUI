// test_bana2.cpp — 调试 Bana 卡号生成
// g++ -O2 -o test_bana2 test_bana2.cpp -lssl -lcrypto && ./test_bana2
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <cstdlib>
#include <openssl/blowfish.h>

static const uint8_t NBGIC0_KEY[56] = {
    0x6B,0xF9,0xE0,0x22,0x4B,0x14,0xA9,0x51,0xD4,0x3D,0xB6,0x2D,0x2A,0x99,
    0x47,0xD8,0xB2,0x78,0x80,0x35,0x8E,0x71,0x16,0xEB,0xA0,0x00,0x01,0x3E,
    0x8C,0xFE,0xF6,0x09,0x02,0xA9,0x59,0x5D,0x4C,0x25,0xFE,0xFF,0x74,0x3F,
    0xC8,0xD8,0xE0,0x75,0x6A,0xE6,0x18,0xEF,0x8B,0x72,0x12,0xDE,0x5A,0x4B,
};

static const uint8_t ACODE_KEY0[56] = {
    0x0f,0x11,0x33,0x28,0x03,0x14,0x2f,0x1d,0x10,0x2a,0x19,0x01,
    0x2b,0x1f,0x1e,0x04,0x13,0x29,0x15,0x2e,0x36,0x0d,0x31,0x09,
    0x2d,0x27,0x1a,0x06,0x16,0x24,0x17,0x23,0x00,0x0c,0x26,0x22,
    0x37,0x32,0x0a,0x30,0x07,0x1c,0x25,0x21,0x05,0x34,0x0e,0x08,
    0x1b,0x35,0x2c,0x0b,0x02,0x20,0x12,0x18,
};
static const uint64_t NUMKEY0 = 0xB2456B04BA6B2300ULL;
#define KEYLEN 56

static void bf_decrypt(const uint8_t *key, size_t keylen, uint8_t *data) {
    BF_KEY bf;
    BF_set_key(&bf, keylen, key);
    uint32_t *p = (uint32_t*)data;
    p[0] = _byteswap_ulong(p[0]);
    p[1] = _byteswap_ulong(p[1]);
    BF_ecb_encrypt((uint8_t*)p, (uint8_t*)p, &bf, BF_DECRYPT);
    p[0] = _byteswap_ulong(p[0]);
    p[1] = _byteswap_ulong(p[1]);
}

// 完全照搬原始 SetBitRange
static void SetBitRange(uint8_t *buf, int nStart, size_t nBits, int dwData) {
    for (size_t i = 0; i < nBits; i++) {
        int idx = ((nStart + i) >> 3) + 1;
        int bit = 7 - ((nStart + i) % 8);
        unsigned int newByte = (1 << bit) | buf[idx];
        if ((dwData & (1LL << (nBits - 1 - i))) == 0)
            newByte = ~newByte;
        buf[idx] = newByte;
    }
}

int main() {
    // 1. 解密
    uint8_t enc[8] = {0x56, 0xEF, 0xCA, 0xB6, 0x57, 0x63, 0x1F, 0x6A};
    bf_decrypt(NBGIC0_KEY, 56, enc);
    printf("Decrypted: ");
    for (int i = 0; i < 8; i++) printf("%02X ", enc[i]);
    uint8_t chk = 0;
    for (int i = 0; i < 7; i++) chk ^= enc[i];
    printf(" | chk=0x%02X %s\n", chk, chk == enc[7] ? "OK" : "FAIL");
    if (chk != enc[7]) return 1;

    // 2. 提取字段
    uint32_t namcoId = *(uint32_t*)enc;
    printf("NamcoID = %u (0x%08X)\n", namcoId, namcoId);

    // 3. 生成访问码
    uint32_t prod = 300;
    uint32_t appdata = 0;

    // BCC
    uint8_t bcc = 0;
    for (int i = 0; i < 4; i++) {
        bcc ^= ((uint8_t*)&prod)[i];
        bcc ^= ((uint8_t*)&appdata)[i];
        bcc ^= ((uint8_t*)&namcoId)[i];
    }
    printf("BCC = 0x%02X\n", bcc);

    // prod = ((prod & 3) << 8) | (prod >> 2)
    prod = ((prod & 3) << 8) | (uint8_t)(prod >> 2);
    printf("prod_new = %u (0x%X)\n", prod, prod);

    // 设位
    uint8_t data[9] = {};
    SetBitRange(data, 0,  2,  (bcc % 11) & 3);              // RandNum
    SetBitRange(data, 2,  10, prod);                         // Product
    SetBitRange(data, 12, 4,  appdata);                      // AppData
    SetBitRange(data, 16, 32, _byteswap_ulong(namcoId));     // NamcoID (big-endian)
    SetBitRange(data, 48, 8,  bcc);                          // BCC

    printf("Encoded: ");
    for (int i = 1; i <= 8; i++) printf("%02X ", data[i]);
    printf("\n");

    // 置换
    uint8_t temp[9] = {};
    for (int i = 0; i < KEYLEN; i++) {
        int bit = 7 - (i % 8);
        int byteIdx = (i >> 3) + 1;
        if ((data[byteIdx] & (1 << bit)) != 0) {
            int idx = ACODE_KEY0[i] % KEYLEN;
            temp[(idx >> 3) + 1] |= (1 << (7 - (idx % 8)));
        }
    }

    printf("Permuted: ");
    for (int i = 1; i <= 8; i++) printf("%02X ", temp[i]);
    printf("\n");

    // U64
    uint64_t res = 0;
    for (int i = 1; i <= 8; i++) res = (res << 8) | temp[i];
    printf("U64 = %llu\n", res);

    uint64_t final = res + NUMKEY0;
    printf("+NUMKEY = %llu\n", final);

    // 转字符串
    char out[32];
    int idx = sizeof(out) - 1;
    out[idx] = 0;
    uint64_t v = final;
    while (v > 0 || idx == (int)sizeof(out) - 1) {
        out[--idx] = '0' + (v % 10);
        v /= 10;
    }
    printf("\nAccess Code: 300%s\n", out + idx);
    printf("Expected:    30028606487746856375\n");

    return 0;
}
