// test_bana3.cpp — 完全照搬 uint64 操作
// g++ -O2 -o test_bana3 test_bana3.cpp -lssl -lcrypto && ./test_bana3
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <cstdlib>
#include <openssl/blowfish.h>

typedef uint64_t UINT64;
typedef uint32_t UINT32;

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
static const UINT64 NUMKEY0 = 0xB2456B04BA6B2300ULL;
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

static void SetBitRange(uint8_t *buf, int nStart, size_t nBits, int dwData) {
    for (size_t i = 0; i < nBits; i++) {
        int idx = ((nStart + i) >> 3) + 1;
        int bit = 7 - ((nStart + i) % 8);
        unsigned int newByte = (1 << bit) | buf[idx];
        if ((dwData & (1LL << (nBits - 1 - i))) == 0)
            newByte = ~newByte;
        buf[idx] = newByte & 0xFF;
    }
}

// 逐字节能看到原始代码的 UINT64 操作
static void UINT64ToStr(char *lpStr, UINT64 *pU64) {
    UINT64 tmp = *pU64;
    char buf[64];
    char *tail = buf;
    bool zero;
    do {
        zero = true;
        UINT32 carry = 0;
        for (int i = 0; i < 8; i++) {
            UINT32 val = (carry << 8) + ((uint8_t*)&tmp)[i];
            ((uint8_t*)&tmp)[i] = val / 10;
            carry = val % 10;
            if (((uint8_t*)&tmp)[i] != 0) zero = false;
        }
        *tail++ = carry + '0';
    } while (!zero);

    // 补足 17 位
    int len = tail - buf;
    for (; len < 17; len++) *lpStr++ = '0';
    while (tail != buf) *lpStr++ = *--tail;
    *lpStr = 0;
}

int main() {
    uint8_t enc[8] = {0x56, 0xEF, 0xCA, 0xB6, 0x57, 0x63, 0x1F, 0x6A};
    bf_decrypt(NBGIC0_KEY, 56, enc);
    printf("Decrypted: ");
    for (int i = 0; i < 8; i++) printf("%02X ", enc[i]);
    uint8_t chk = 0; for (int i = 0; i < 7; i++) chk ^= enc[i];
    printf("| chk=0x%02X %s\n", chk, chk == enc[7] ? "OK" : "FAIL");

    UINT32 namcoId = *(UINT32*)enc;
    printf("NamcoID = %u\n", namcoId);

    UINT32 prod = 300, appdata = 0;
    uint8_t bcc = 0;
    for (int i = 0; i < 4; i++) {
        bcc ^= ((uint8_t*)&prod)[i];
        bcc ^= ((uint8_t*)&appdata)[i];
        bcc ^= ((uint8_t*)&namcoId)[i];
    }
    prod = ((prod & 3) << 8) | (uint8_t)(prod >> 2);
    printf("BCC=0x%02X prod_new=%u\n", bcc, prod);

    uint8_t data[9] = {};
    SetBitRange(data, 0,  2,  (bcc % 11) & 3);
    SetBitRange(data, 2,  10, prod);
    SetBitRange(data, 12, 4,  appdata);
    // Try without swap
    SetBitRange(data, 16, 32, namcoId);  // NO swap
    SetBitRange(data, 48, 8,  bcc);

    uint8_t temp[9] = {};
    for (int i = 0; i < KEYLEN; i++) {
        int bit = 7 - (i % 8);
        int bIdx = (i >> 3) + 1;
        if (data[bIdx] & (1 << bit)) {
            int idx = ACODE_KEY0[i] % KEYLEN;
            temp[(idx >> 3) + 1] |= (1 << (7 - (idx % 8)));
        }
    }

    // memcpy(temp bytes 1..8 into UINT64 (like IDDataToUINT64)
    UINT64 res = 0;
    memcpy(&res, temp + 1, 8);
    printf("res bytes: ");
    for (int i = 0; i < 8; i++) printf("%02X ", ((uint8_t*)&res)[i]);
    printf("\n");

    // UINT64Add: byte by byte, 7→0 with carry
    UINT64 numkey = NUMKEY0;
    UINT64 sum = 0;
    unsigned carry = 0;
    for (int i = 7; i >= 0; i--) {
        carry += ((uint8_t*)&res)[i] + ((uint8_t*)&numkey)[i];
        ((uint8_t*)&sum)[i] = carry;
        carry >>= 8;
    }
    printf("sum bytes: ");
    for (int i = 0; i < 8; i++) printf("%02X ", ((uint8_t*)&sum)[i]);
    printf("\n");

    char ac[32];
    UINT64ToStr(ac, &sum);
    printf("\nAccess Code: 300%s\n", ac);
    printf("Expected:    30028606487746856375\n");
    return 0;
}
