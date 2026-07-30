#ifndef BANAPASSCARD_H
#define BANAPASSCARD_H

#include <cstdint>
#include <QtCore/QByteArray>
#include <QtCore/QString>

/* ================================================================
 *  Banapass 卡号解析 — 从 Python banapass_access_code.py 移植
 *
 *  Block1 (16 bytes): [ver:2][name:6][encrypted:8]
 *  解密后得到 NamcoID, 再通过置换 + 偏移得到 20 位卡号
 * ================================================================ */

// Blowfish 解密 Block1 后 8 字节, 返回 NamcoID (序列号)
// 成功返回 NamcoID, 失败返回 -1
int32_t banapassDecode(const uint8_t block1[16], int *keynumOut = nullptr);

// 由 NamcoID + 密钥编号生成 20 位卡号
QString banapassEncodeAccessCode(int keynum, uint32_t namcoId);

/* ================================================================
 *  IDM 转卡号 — 用于 AIC / Other Felica 卡
 *
 *  输入: 8 字节 UID (Card_RawData[0] 前 8 字节)
 *  输出: 20 位数字卡号 (QString)
 * ================================================================ */
QString idmToAccessCode(const uint8_t uid[8]);

#endif // BANAPASSCARD_H
