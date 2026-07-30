// test_card2.c — 测试第二张卡
#include <stdio.h>
#include <string.h>
#include "nbgi_types.h"

int NbgiGetAccessCode(char *AccessCode, const void *HeaderData, int *NbgiSerial);

int main() {
    // 新卡 Block 1: 00024E42474943367316FCE153F27757
    BYTE header[16] = {
        0x00, 0x02,
        0x4E, 0x42, 0x47, 0x49, 0x43, 0x36,  // "NBGIC6"
        0x73, 0x16, 0xFC, 0xE1, 0x53, 0xF2, 0x77, 0x57,  // encrypted
    };

    char accessCode[32];
    int nbgiSerial = 0;
    int ret = NbgiGetAccessCode(accessCode, header, &nbgiSerial);
    printf("Returns: %d\n", ret);
    printf("NamcoID: %d (0x%08X)\n", nbgiSerial, nbgiSerial);
    printf("Access Code: %s\n", accessCode);

    // Block 2 明文卡号
    BYTE block2[16] = {
        0x00,0x00,0x00,0x00,0x00,0x00,
        0x30,0x77,0x30,0x60,0x78,0x36,0x96,0x98,0x35,0x28
    };
    printf("\nBlock2 plain (hex bytes 6-15): ");
    for (int i=6;i<16;i++) printf("%02X", block2[i]);
    printf("\n");

    return 0;
}
