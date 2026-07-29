#ifndef FLASHDATA_H
#define FLASHDATA_H

#include <cstdint>

/* FlashData - 读卡器 Flash 配置数据结构
 *
 * 16字节 union，支持 word / byte / 字段 三种访问方式。
 * Bytes 0-6 为有效配置区, bytes 7-15 预留。
 *
 * Byte 0  : firmversion_1      固件版本 低字节 (read-only from PC)
 * Byte 1  : firmversion_2      固件版本 高字节 (read-only from PC)
 * Byte 2  : system_setting     UART 波特率: 0=115200, 1=38400, 2=9600
 * Byte 3  : gobal_LED_setting  全局 LED 亮度 (0-255)
 * Byte 4  : sega_setting       SEGA 卡种支持设置 (bit mask)
 * Byte 5  : spice_setting      SPICE 卡种支持设置 (bit mask)
 * Byte 6  : namco_setting      NAMCO 卡种支持设置 (bit mask)
 *
 * sega_setting:
 *   BIT0: tn32 emulator enable          0: disable (15693)
 *   BIT1: FeliCa AIC → real serial      0: disable  1: enable (SEGA mode)
 *   BIT2: unused
 *   BIT3: NESiCA (MIFARE Ultralight)    0: disable
 *   BIT4: e-Amusement Pass (ISO15693)   0: disable
 *   BIT5: T-Union 交通联合 (14443-A)    0: disable
 *   BIT6: Jubeat China 乐动魔方 (MIFARE) 0: disable
 *
 * spice_setting:
 *   BIT0: 2P mode enable                0: 1P
 *   BIT1: Classic Aime (MIFARE)         0: disable
 *   BIT2: Banapass (MIFARE)             0: disable
 *   BIT3: NESiCA (MIFARE Ultralight)    0: disable
 *   BIT4: unused
 *   BIT5: T-Union 交通联合 (14443-A)    0: disable
 *   BIT6: Jubeat China 乐动魔方 (MIFARE) 0: disable
 *
 * namco_setting:
 *   BIT0: (reserved)
 *   BIT1: Classic Aime (MIFARE)         0: disable
 *   BIT2: Banapass (MIFARE)             0: disable
 *   BIT3: NESiCA (MIFARE Ultralight)    0: disable
 *   BIT4: unused
 *   BIT5: T-Union 交通联合 (14443-B)    0: disable
 *   BIT6: Jubeat China 乐动魔方 (MIFARE) 0: disable
 */
typedef union {
    uint32_t raw_flash_word[4];
    uint8_t  raw_flash_byte[16];
    struct {
        uint8_t firmversion_1;       // offset 0
        uint8_t firmversion_2;       // offset 1
        uint8_t system_setting;      // offset 2 — UART 波特率
        uint8_t gobal_LED_setting;   // offset 3 — LED 亮度
        uint8_t sega_setting;        // offset 4
        uint8_t spice_setting;       // offset 5
        uint8_t namco_setting;       // offset 6
    };
} FlashData;

#endif // FLASHDATA_H
