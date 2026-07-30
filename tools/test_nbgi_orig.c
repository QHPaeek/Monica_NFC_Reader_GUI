// test_nbgi_orig.c — 直接调用 micetools 源码
#include <stdio.h>
#include <string.h>
#include "nbgi_types.h"

// 原型声明
int NbgiGetAccessCode(char *AccessCode, const void *HeaderData, int *NbgiSerial);

int main() {
    // 用户提供的 Bana 卡 HeaderData (Card_RawData[1])
    BYTE header[16] = {
        0x00, 0x02,                           // version 0x0200
        0x4E, 0x42, 0x47, 0x49, 0x43, 0x30, // "NBGIC0"
        0x56, 0xEF, 0xCA, 0xB6, 0x57, 0x63, 0x1F, 0x6A, // encrypted
    };

    char accessCode[32];
    int nbgiSerial = 0;

    int ret = NbgiGetAccessCode(accessCode, header, &nbgiSerial);
    printf("NbgiGetAccessCode returns: %d\n", ret);
    printf("Access Code: %s\n", accessCode);
    printf("NBGI Serial: %d (0x%08X)\n", nbgiSerial, nbgiSerial);
    printf("Expected:    30028606487746856375\n");
    return 0;
}
