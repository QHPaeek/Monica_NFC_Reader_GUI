#include "nbgi_types.h"

/* Structs */
#pragma pack(push, 1)
typedef struct struc_1 {
    uint32_t m_Serial;
    uint16_t Unk04;
    uint8_t  Unk06;
    uint8_t  m_KeyNum;
} struc_1;

#define KEYLEN 56
typedef struct BLOWFISH_KEY {
    char m_Name[8];
    uint8_t m_Key[KEYLEN];
    uint8_t Unk40[72];
} BLOWFISH_KEY, *PBLOWFISH_KEY;

typedef struct AC_KEY {
    uint32_t m_dwPrefix;
    char     m_szPrefix[4];
    uint8_t  m_Key[KEYLEN];
    UINT64   m_NumKey;
} AC_KEY, *PAC_KEY;
#pragma pack(pop)

/* BNA functions */
int BNACCardDecode(struc_1 *Decoded, const void *HeaderData);
int BNACCardEncodeAccessCode(char *AccessCode, BYTE KeyNum, UINT32 NamcoID);

/* Exports */
int NbgiGetAccessCode(char *AccessCode, const void *HeaderData, int *NbgiSerial);
void BNACCardBFDec(LPBYTE lpKey, size_t nBytes, LPBYTE lpData);

/* TODO: (felica stuff) */
// int NbgiFelicaLiteAuth(LPVOID IDmPMm, LPVOID RC, LPVOID InData, LPVOID Mac, LPVOID MKey);
// void MakeCardKey(LPVOID IDmPMm, LPVOID lpMac);
// void CMAC(LPVOID dataIn, byte *xorKey, LPVOID macOut);
// void MakeSessionKey(byte *mac, byte *param_2, LPVOID param_3);
// void MakeMac(int param_1, byte *param_2, byte *param_3, byte *param_4);
