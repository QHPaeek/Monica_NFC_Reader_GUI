// test_banapass.cpp — 验证 Banapass 卡号解析算法
// 编译: g++ -O2 -o test_banapass test_banapass.cpp -lssl -lcrypto
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <cstdlib>
#include <openssl/blowfish.h>

// NBGIC0 密钥 (56 字节)
static const uint8_t NBGIC0_KEY[56] = {
    0x6B,0xF9,0xE0,0x22,0x4B,0x14,0xA9,0x51,0xD4,0x3D,0xB6,0x2D,0x2A,0x99,
    0x47,0xD8,0xB2,0x78,0x80,0x35,0x8E,0x71,0x16,0xEB,0xA0,0x00,0x01,0x3E,
    0x8C,0xFE,0xF6,0x09,0x02,0xA9,0x59,0x5D,0x4C,0x25,0xFE,0xFF,0x74,0x3F,
    0xC8,0xD8,0xE0,0x75,0x6A,0xE6,0x18,0xEF,0x8B,0x72,0x12,0xDE,0x5A,0x4B,
};

// 访问码生成用的 s_IDKey (只取 NBGIC0 的)
static const uint8_t ACODE_KEY0[56] = {
    0x0f,0x11,0x33,0x28,0x03,0x14,0x2f,0x1d,0x10,0x2a,0x19,0x01,
    0x2b,0x1f,0x1e,0x04,0x13,0x29,0x15,0x2e,0x36,0x0d,0x31,0x09,
    0x2d,0x27,0x1a,0x06,0x16,0x24,0x17,0x23,0x00,0x0c,0x26,0x22,
    0x37,0x32,0x0a,0x30,0x07,0x1c,0x25,0x21,0x05,0x34,0x0e,0x08,
    0x1b,0x35,0x2c,0x0b,0x02,0x20,0x12,0x18,
};
static const uint64_t NUMKEY0 = 0xB2456B04BA6B2300ULL;
static const char PREFIX0[] = "300";
#define KEYLEN 56

static void bf_decrypt(const uint8_t *key, size_t keylen, uint8_t *data, size_t blocks) {
    BF_KEY bf;
    BF_set_key(&bf, keylen, key);
    for (size_t i = 0; i < blocks; i++) {
        uint32_t *p = (uint32_t*)(data + i * 8);
        p[0] = _byteswap_ulong(p[0]);
        p[1] = _byteswap_ulong(p[1]);
        BF_ecb_encrypt((uint8_t*)p, (uint8_t*)p, &bf, BF_DECRYPT);
        p[0] = _byteswap_ulong(p[0]);
        p[1] = _byteswap_ulong(p[1]);
    }
}

static void set_bit_range(uint8_t *buf, int start, size_t nbits, uint32_t val) {
    for (size_t i = 0; i < nbits; i++) {
        unsigned int bt = (1 << (7 - (start + i) % 8)) | buf[((start + i) >> 3) + 1];
        if ((val & (1 << (nbits - 1 - i))) == 0) bt = ~bt;
        buf[((start + i) >> 3) + 1] = bt & 0xff;
    }
}

static uint64_t id_data_to_u64(const uint8_t *data) {
    // data[0]=0, data[1..8] is the 8-byte payload
    uint64_t r = 0;
    for (int i = 1; i <= 8; i++) {
        r = (r << 8) | data[i];
    }
    return r;
}

static void u64_to_str(char *out, uint64_t v) {
    // 输出为 17 位十进制字符串
    char buf[32];
    int idx = sizeof(buf) - 1;
    buf[idx] = 0;
    while (v > 0 || idx == (int)sizeof(buf) - 1) {
        buf[--idx] = '0' + (v % 10);
        v /= 10;
    }
    strcpy(out, buf + idx);
}

int main() {
    // 用户提供的 Bana 卡 RawData[1] (16 字节)
    // 从第二行: 0002 4E42 4749 4330 56EF CAB6 5763 1F6A
    uint8_t header[16] = {
        0x00, 0x02,                           // version = 0x0200
        0x4E, 0x42, 0x47, 0x49, 0x43, 0x30, // "NBGIC0"
        0x56, 0xEF, 0xCA, 0xB6, 0x57, 0x63, 0x1F, 0x6A, // encrypted
    };

    // 1. 检查版本号
    uint16_t ver = *(uint16_t*)header;
    printf("Version: 0x%04X (%s)\n", ver, ver == 0x0200 ? "OK" : "FAIL");
    if (ver != 0x0200) return 1;

    // 2. 检查密钥名
    char name[7] = {};
    memcpy(name, header + 2, 6);
    printf("KeyName: \"%s\" (%s)\n", name, strcmp(name, "NBGIC0") == 0 ? "OK" : "FAIL");

    // 3. Blowfish 解密后 8 字节
    uint8_t dec[8];
    memcpy(dec, header + 8, 8);
    bf_decrypt(NBGIC0_KEY, 56, dec, 1);
    printf("Decrypted: ");
    for (int i = 0; i < 8; i++) printf("%02X ", dec[i]);
    printf("\n");

    // 4. 校验: dec[0..6] XOR = dec[7]
    uint8_t chk = 0;
    for (int i = 0; i < 7; i++) chk ^= dec[i];
    printf("Checksum: calc=0x%02X stored=0x%02X (%s)\n", chk, dec[7], chk == dec[7] ? "OK" : "FAIL");

    // 5. 提取 NamcoID (序列号)
    uint32_t namcoId = *(uint32_t*)(dec);
    printf("NamcoID (Serial): 0x%08X = %u\n", namcoId, namcoId);

    // 6. 生成访问码 (20 位卡号)
    uint32_t prod = 300;   // NBGIC0 prefix
    uint32_t appdata = 0;

    // BCC
    uint8_t bcc = 0;
    for (int i = 0; i < 4; i++) {
        bcc ^= ((uint8_t*)&prod)[i];
        bcc ^= ((uint8_t*)&appdata)[i];
        bcc ^= ((uint8_t*)&namcoId)[i];
    }

    // prod = ((prod & 3) << 8) | (prod >> 2)
    prod = ((prod & 3) << 8) | (uint8_t)(prod >> 2);

    uint8_t data[9] = {};  // data[0]=0, data[1..8]=payload
    set_bit_range(data, 0,  2,  (bcc % 11) & 3);    // RandNum
    set_bit_range(data, 2,  10, prod);                // Product
    set_bit_range(data, 12, 4,  appdata);             // AppData
    set_bit_range(data, 16, 32, _byteswap_ulong(namcoId)); // NamcoID (big-endian)
    set_bit_range(data, 48, 8,  bcc);                 // BCC

    printf("Encoded bits: ");
    for (int i = 1; i <= 8; i++) printf("%02X ", data[i]);
    printf("\n");

    // 置换
    uint8_t temp[9] = {};
    for (int i = 0; i < KEYLEN; i++) {
        if (((1 << (7 - i % 8)) & data[(i >> 3) + 1]) != 0) {
            int idx = ACODE_KEY0[i] % KEYLEN;
            temp[(idx >> 3) + 1] |= (1 << (7 - (idx % 8)));
        }
    }

    printf("Permuted: ");
    for (int i = 1; i <= 8; i++) printf("%02X ", temp[i]);
    printf("\n");

    uint64_t res = id_data_to_u64(temp);
    printf("U64 result: %llu (0x%llX)\n", res, res);

    // res += numkey
    uint64_t final = res + NUMKEY0;
    printf("Final u64: %llu\n", final);

    char access[32];
    u64_to_str(access, final);
    printf("\nAccess Code: %s%s\n", PREFIX0, access);

    return 0;
}
