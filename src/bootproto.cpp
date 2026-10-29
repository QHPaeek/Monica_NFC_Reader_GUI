#include "bootproto.h"

#include <QtSerialPort/QSerialPort>
#include <QtSerialPort/QSerialPortInfo>
#include <QtEndian>
#include <QFile>
#include <QFileInfo>
#include <QElapsedTimer>
#include <QThread>

#include <cstring>

/* --------------------------------------------------------------------
 *  小工具
 * -------------------------------------------------------------------- */

namespace
{

const QByteArray &frameMagic()
{
    /* 0x42464641 的小端字节序列 */
    static const QByteArray magic = QByteArray::fromHex("41464642");
    return magic;
}

void putU16(QByteArray &out, quint16 v)
{
    char buf[2];
    qToLittleEndian(v, buf);
    out.append(buf, 2);
}

void putU32(QByteArray &out, quint32 v)
{
    char buf[4];
    qToLittleEndian(v, buf);
    out.append(buf, 4);
}

quint16 getU16(const char *p)
{
    return qFromLittleEndian<quint16>(reinterpret_cast<const uchar *>(p));
}

quint32 getU32(const char *p)
{
    return qFromLittleEndian<quint32>(reinterpret_cast<const uchar *>(p));
}

} // namespace

/* --------------------------------------------------------------------
 *  固件镜像形态判断
 * -------------------------------------------------------------------- */

namespace
{

/* 某个偏移处是否是一颗合理的 Cortex-M 向量表:
 *   [0] = 初始 MSP, 必须落在 SRAM (0x2000xxxx)
 *   [1] = Reset_Handler, 必须落在指定 Flash 区间内 (忽略 Thumb 位)
 */
bool vectorTableAt(const QByteArray &data, int offset,
                   quint32 flashLo, quint32 flashHi)
{
    if (offset < 0 || data.size() < offset + 8)
        return false;

    const uchar *p = reinterpret_cast<const uchar *>(data.constData()) + offset;
    const quint32 sp = qFromLittleEndian<quint32>(p);
    const quint32 rv = qFromLittleEndian<quint32>(p + 4) & ~1u;

    const bool spOk = ((sp & 0xFFF00000u) == 0x20000000u) && (sp > 0x20000000u);
    const bool rvOk = (rv >= flashLo) && (rv < flashHi);
    return spOk && rvOk;
}

} // namespace

BootImageKind classifyFirmwareImage(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return BootImageKind::Invalid;

    const QByteArray head = f.read(static_cast<qint64>(BOOT_IMAGE_HEADER_SIZE) + 8);
    f.close();

    /* 0x6000 处必须是 APP 向量表 —— 这是"带 BOOTLOADER 头"的硬判据。
     * 纯 APP 镜像在这里是代码中段, 不可能凑巧满足 SP/复位向量两个条件。
     */
    const bool appVecOk = vectorTableAt(head, static_cast<int>(BOOT_IMAGE_HEADER_SIZE),
                                        BOOT_APP_BASE_ADDR, BOOT_FLASH_END_ADDR);
    if (!appVecOk) {
        /* 区分"合法但只有 APP"与"根本不是固件" */
        const bool headVecOk = vectorTableAt(head, 0,
                                             0x08000000u, BOOT_FLASH_END_ADDR);
        return headVecOk ? BootImageKind::AppOnly : BootImageKind::Invalid;
    }

    /* 0x6000 处已经是合法 APP 向量表 → 判定为合并固件 */
    return BootImageKind::Merged;
}

/* --------------------------------------------------------------------
 *  BootFlasher
 * -------------------------------------------------------------------- */

BootFlasher::BootFlasher(QObject *parent)
    : QObject(parent)
{
    m_busy.storeRelease(0);
    m_abort.storeRelease(0);
}

quint32 BootFlasher::crc32(const QByteArray &data)
{
    static quint32 table[256];
    static bool ready = false;

    if (!ready) {
        for (quint32 i = 0; i < 256; ++i) {
            quint32 c = i;
            for (int k = 0; k < 8; ++k)
                c = (c & 1u) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
            table[i] = c;
        }
        ready = true;
    }

    quint32 crc = 0xFFFFFFFFu;
    const uchar *p = reinterpret_cast<const uchar *>(data.constData());
    for (int i = 0; i < data.size(); ++i)
        crc = table[(crc ^ p[i]) & 0xFFu] ^ (crc >> 8);
    return crc ^ 0xFFFFFFFFu;
}

QString BootFlasher::errorName(quint32 code)
{
    static const char *names[] = {
        "NONE", "BAD_MAGIC", "BAD_HEADER_CRC", "BAD_PAYLOAD_CRC",
        "UNSUPPORTED_OPCODE", "BAD_STATE", "BAD_TARGET", "RANGE",
        "ALIGNMENT", "FLASH", "IMAGE_TOO_LARGE", "IMAGE_CRC",
        "NO_VALID_APP", "MANIFEST", "SIGNATURE", "ROLLBACK",
        "DECRYPTION", "UPGRADE_PATH", "FRAME_TIMEOUT",
    };
    if (code < sizeof(names) / sizeof(names[0]))
        return QString::fromLatin1(names[code]);
    return QStringLiteral("UNKNOWN(%1)").arg(code);
}

QString BootFlasher::statusText(const QByteArray &payload)
{
    if (payload.size() < 20)
        return QStringLiteral("(status payload too short)");

    const quint32 status = getU32(payload.constData());
    const quint32 err    = getU32(payload.constData() + 4);
    const quint32 wrote  = getU32(payload.constData() + 8);
    const quint32 expect = getU32(payload.constData() + 12);

    static const char *statusNames[] = {
        "IDLE", "READY", "RECEIVING", "COMMITTED", "ERROR", "ERASING", "VERIFYING"
    };
    const QString st = (status < 7)
        ? QString::fromLatin1(statusNames[status])
        : QStringLiteral("%1").arg(status);

    if (err != 0)
        return QStringLiteral("status=%1 error=%2").arg(st, errorName(err));
    return QStringLiteral("status=%1 %2/%3").arg(st).arg(wrote).arg(expect);
}

/* ---------------- 帧收发 ---------------- */

bool BootFlasher::writeFrame(QSerialPort &port, quint16 opcode, quint16 seq,
                             const QByteArray &payload)
{
    QByteArray header;
    header.reserve(16);
    putU32(header, BOOT_FRAME_MAGIC);
    putU16(header, opcode);
    putU16(header, seq);
    putU32(header, static_cast<quint32>(payload.size()));
    putU32(header, crc32(payload));

    QByteArray frame = header;
    putU32(frame, crc32(header));
    frame += payload;

    if (port.write(frame) != frame.size())
        return false;
    port.flush();
    port.waitForBytesWritten(1000);
    return true;
}

bool BootFlasher::readFrame(QSerialPort &port, quint16 expectOpcode,
                            QByteArray *payload, int timeoutMs, QString *err)
{
    QByteArray buf;
    QElapsedTimer timer;
    timer.start();

    for (;;) {
        /* 1. 对齐到 magic */
        while (buf.size() >= 4 && buf.left(4) != frameMagic()) {
            if (m_abort.loadAcquire()) {
                if (err) *err = QStringLiteral("已取消");
                return false;
            }
            buf.remove(0, 1);
        }

        /* 2. 头部完整后校验 */
        if (buf.size() >= 20) {
            const quint16 op   = getU16(buf.constData() + 4);
            const quint32 plen = getU32(buf.constData() + 8);
            const quint32 pcrc = getU32(buf.constData() + 12);
            const quint32 hcrc = getU32(buf.constData() + 16);

            const bool headerOk =
                (crc32(buf.left(16)) == hcrc) &&
                (plen <= static_cast<quint32>(BOOT_FRAME_MAX_PAYLOAD));

            if (!headerOk) {
                buf.remove(0, 1);
                continue;
            }

            if (buf.size() >= 20 + static_cast<int>(plen)) {
                const QByteArray body = buf.mid(20, static_cast<int>(plen));
                if (crc32(body) != pcrc) {
                    buf.remove(0, 1);
                    continue;
                }

                buf.remove(0, 20 + static_cast<int>(plen));

                /* 只看目标应答, 其他帧丢弃 */
                if ((op & ~BOOT_OP_RESPONSE) != expectOpcode)
                    continue;

                if (payload) *payload = body;
                return true;
            }
        }

        /* 3. 需要更多数据 */
        const int remain = timeoutMs - static_cast<int>(timer.elapsed());
        if (remain <= 0) {
            if (err) {
                *err = QStringLiteral("等待应答超时 (%1 ms, 已收到 %2 字节)")
                           .arg(timeoutMs).arg(buf.size());
            }
            return false;
        }
        port.waitForReadyRead(qMin(remain, 100));
        buf += port.readAll();
    }
}

bool BootFlasher::transact(QSerialPort &port, quint16 opcode, quint16 seq,
                           const QByteArray &payload, QByteArray *response,
                           int timeoutMs, QString *err)
{
    port.clear(QSerialPort::Input);

    if (!writeFrame(port, opcode, seq, payload)) {
        if (err) *err = QStringLiteral("串口写入失败");
        return false;
    }
    return readFrame(port, opcode, response, timeoutMs, err);
}

/* ---------------- 打开串口 / 握手 ---------------- */

bool BootFlasher::openPort(QSerialPort &port, const QString &name, QString *err)
{
    if (port.isOpen())
        port.close();

    port.setPortName(name);
    port.setBaudRate(115200);
    port.setDataBits(QSerialPort::Data8);
    port.setParity(QSerialPort::NoParity);
    port.setStopBits(QSerialPort::OneStop);
    port.setFlowControl(QSerialPort::NoFlowControl);

    QString last;
    for (int i = 0; i < 10; ++i) {
        if (port.open(QIODevice::ReadWrite)) {
            port.clear(QSerialPort::AllDirections);

            /* 只对真正的 USB CDC 拉高 DTR。
             * UART 转接板的 DTR 常被接到 RST, 乱拉会把板子按在复位上。
             */
            const QSerialPortInfo info(name);
            if (info.hasVendorIdentifier() && info.hasProductIdentifier()
                && info.vendorIdentifier() == 0xAFF1
                && info.productIdentifier() == 0x52B2) {
                port.setDataTerminalReady(true);
                QThread::msleep(20);
            }
            return true;
        }
        last = port.errorString();
        QThread::msleep(100);
    }

    if (err) {
        *err = QStringLiteral("无法打开 %1: %2").arg(name, last);
    }
    return false;
}

bool BootFlasher::handshake(QSerialPort &port, BootHello *hello,
                            BootDeviceInfo *info, QString *err)
{
    /* --- HELLO --- */
    QByteArray rsp;
    if (!transact(port, BOOT_OP_HELLO, 1, QByteArray(), &rsp, 500, err))
        return false;

    if (rsp.size() != 28) {
        if (err) *err = QStringLiteral("HELLO 应答长度异常 (%1)").arg(rsp.size());
        return false;
    }

    if (hello) {
        hello->protocolVersion = getU32(rsp.constData() + 0);
        hello->targetId        = getU32(rsp.constData() + 4);
        hello->flashSize       = getU32(rsp.constData() + 8);
        hello->appBase         = getU32(rsp.constData() + 12);
        hello->slotSize        = getU32(rsp.constData() + 16);
        hello->maxChunkSize    = getU32(rsp.constData() + 20);
        hello->capabilities    = getU32(rsp.constData() + 24);
    }

    /* --- GET_DEVICE_INFO --- */
    if (!transact(port, BOOT_OP_GET_DEVICE_INFO, 2, QByteArray(), &rsp, 800, err))
        return false;

    if (rsp.size() != 56) {
        if (err) *err = QStringLiteral("DEVICE_INFO 应答长度异常 (%1)").arg(rsp.size());
        return false;
    }

    if (info) {
        info->protocolVersion = getU32(rsp.constData() + 0);
        info->targetId        = getU32(rsp.constData() + 4);
        info->boardId         = getU32(rsp.constData() + 8);
        /* +12 board_revision, +16 flash_layout_id */
        info->flashLayoutId   = getU32(rsp.constData() + 16);
        /* +20 flash_base, +24 flash_size */
        info->appBase         = getU32(rsp.constData() + 28);
        info->metadataBase    = getU32(rsp.constData() + 32);
        info->metadataSize    = getU32(rsp.constData() + 36);
        /* +40 erase_size */
        info->writeSize       = getU32(rsp.constData() + 44);
        info->maxChunkSize    = getU32(rsp.constData() + 48);
    }

    return true;
}

/* ---------------- 探测 ---------------- */

void BootFlasher::probe(const QString &portName)
{
    m_busy.storeRelease(1);
    m_abort.storeRelease(0);

    QSerialPort port;
    QString err;

    if (!openPort(port, portName, &err)) {
        m_busy.storeRelease(0);
        emit probeFinished(false, err);
        return;
    }

    BootHello hello;
    BootDeviceInfo info;
    if (!handshake(port, &hello, &info, &err)) {
        port.close();
        m_busy.storeRelease(0);
        emit probeFinished(false, err);
        return;
    }
    port.close();
    m_busy.storeRelease(0);

    const QString summary = QStringLiteral(
        "协议 0x%1 · target 0x%2 · board 0x%3 · APP 0x%4 · chunk %5")
        .arg(hello.protocolVersion, 8, 16, QLatin1Char('0'))
        .arg(hello.targetId, 8, 16, QLatin1Char('0'))
        .arg(info.boardId, 8, 16, QLatin1Char('0'))
        .arg(hello.appBase, 8, 16, QLatin1Char('0'))
        .arg(hello.maxChunkSize);

    emit probeFinished(hello.protocolVersion == BOOT_PROTOCOL_VERSION, summary);
}

/* ---------------- 烧录 ---------------- */

void BootFlasher::flash(const QString &portName, const QString &imagePath,
                        bool skipBootHeader)
{
    m_busy.storeRelease(1);
    m_abort.storeRelease(0);

    QSerialPort port;

    auto fail = [&](const QString &msg) {
        if (port.isOpen())
            port.close();
        m_busy.storeRelease(0);
        emit flashFinished(false, msg);
    };

    /* --- 读取固件 --- */
    QFile file(imagePath);
    if (!file.open(QIODevice::ReadOnly)) {
        fail(QStringLiteral("无法打开固件文件: %1").arg(imagePath));
        return;
    }
    const QByteArray raw = file.readAll();
    file.close();

    if (raw.isEmpty()) {
        fail(QStringLiteral("固件文件为空"));
        return;
    }

    const int rawSize = raw.size();
    if (skipBootHeader) {
        /* 硬性要求: 只有「BOOTLOADER + APP」合并固件才能走 BOOT 通道。
         * 纯 APP 镜像(老固件)在 0x6000 处不是向量表, 跳过 24 KiB 会直接刷坏。
         */
        const BootImageKind kind = classifyFirmwareImage(imagePath);
        if (kind != BootImageKind::Merged) {
            fail(kind == BootImageKind::AppOnly
                ? QStringLiteral("该固件不带 BOOTLOADER 头（0x6000 处不是合法的 APP 向量表），"
                                 "不能通过 BOOT 通道烧录。请改用 DFU 方式。")
                : QStringLiteral("文件不是有效的固件镜像（向量表不合法），已中止。"));
            return;
        }

        if (rawSize <= static_cast<int>(BOOT_IMAGE_HEADER_SIZE)) {
            fail(QStringLiteral("文件只有 %1 字节, 比 BOOTLOADER 头还小, "
                                "没有可烧录的 APP 区").arg(rawSize));
            return;
        }
    }

    /* --- 打开串口 + 握手 --- */
    QString err;
    if (!openPort(port, portName, &err)) {
        fail(err);
        return;
    }
    emit logLine(QStringLiteral("串口 %1 已打开").arg(portName));

    BootHello hello;
    BootDeviceInfo info;
    if (!handshake(port, &hello, &info, &err)) {
        fail(QStringLiteral("握手失败: %1").arg(err));
        return;
    }

    emit logLine(QStringLiteral(
        "HELLO ok: 协议 0x%1, target 0x%2, APP 基址 0x%3, slot %4, chunk %5")
        .arg(hello.protocolVersion, 8, 16, QLatin1Char('0'))
        .arg(hello.targetId, 8, 16, QLatin1Char('0'))
        .arg(hello.appBase, 8, 16, QLatin1Char('0'))
        .arg(hello.slotSize).arg(hello.maxChunkSize));
    emit logLine(QStringLiteral(
        "DEVICE_INFO ok: board 0x%1, layout 0x%2, metadata 0x%3+%4, 写对齐 %5")
        .arg(info.boardId, 8, 16, QLatin1Char('0'))
        .arg(info.flashLayoutId, 8, 16, QLatin1Char('0'))
        .arg(info.metadataBase, 8, 16, QLatin1Char('0'))
        .arg(info.metadataSize)
        .arg(info.writeSize));

    if (hello.protocolVersion != BOOT_PROTOCOL_VERSION) {
        fail(QStringLiteral("协议版本不匹配: 设备 0x%1, 上位机 0x%2")
             .arg(hello.protocolVersion, 8, 16, QLatin1Char('0'))
             .arg(BOOT_PROTOCOL_VERSION, 8, 16, QLatin1Char('0')));
        return;
    }

    /* 设备报告的 APP 基址必须与合并固件的布局一致, 否则跳过 0x6000 就是错的 */
    if (skipBootHeader && hello.appBase != BOOT_APP_BASE_ADDR) {
        fail(QStringLiteral(
            "设备报告的 APP 基址是 0x%1，与预期的 0x%2 不一致，"
            "说明该设备的 Flash 布局与合并固件不匹配，已中止。")
            .arg(hello.appBase, 8, 16, QLatin1Char('0'))
            .arg(static_cast<uint>(BOOT_APP_BASE_ADDR), 8, 16, QLatin1Char('0')));
        return;
    }

    /* --- 组装要写入的镜像 --- */
    QByteArray image;
    if (skipBootHeader) {
        image = raw.mid(static_cast<int>(BOOT_IMAGE_HEADER_SIZE));

        /* 合并固件在 APP 区之后还有一块 metadata (F072: 0x0801F800, 368 字节)。
         * 那是 bootloader 的持久化信息, COMMIT 时它自己会写,
         * 绝不能当数据发过去 —— 否则会超出 APP slot。
         */
        if (info.metadataBase > hello.appBase) {
            const int slotLen = static_cast<int>(info.metadataBase - hello.appBase);
            if (image.size() > slotLen) {
                image.truncate(slotLen);
                emit logLine(QStringLiteral(
                    "已裁掉 slot 之外的尾部数据: APP slot = 0x%1..0x%2 (%3 字节)")
                    .arg(hello.appBase, 8, 16, QLatin1Char('0'))
                    .arg(info.metadataBase, 8, 16, QLatin1Char('0'))
                    .arg(slotLen));
            }

            /* 文件尾部若确实带 metadata, 用它的 image_size 精确截断,
             * 省掉大片 0xFF 填充的无效写入。
             */
            const qint64 mdOffset = static_cast<qint64>(info.metadataBase)
                                    - 0x08000000LL;
            if (info.metadataSize >= 64
                && mdOffset >= 0
                && mdOffset + 64 <= raw.size()) {
                const uchar *m = reinterpret_cast<const uchar *>(raw.constData())
                                 + mdOffset;
                const quint32 magic   = qFromLittleEndian<quint32>(m);
                const quint32 imgSize = qFromLittleEndian<quint32>(m + 20);
                const quint32 imgCrc  = qFromLittleEndian<quint32>(m + 24);

                if (magic == 0x4D544641u
                    && imgSize > 0
                    && imgSize <= static_cast<quint32>(image.size())) {
                    image.truncate(static_cast<int>(imgSize));
                    emit logLine(QStringLiteral(
                        "按 metadata 定长: APP %1 字节, 声明 CRC32 0x%2")
                        .arg(imgSize)
                        .arg(imgCrc, 8, 16, QLatin1Char('0')));

                    const quint32 realCrc = crc32(image);
                    if (realCrc != imgCrc) {
                        emit logLine(QStringLiteral(
                            "⚠ metadata 声明的 CRC32 与实际不符 (实际 0x%1)，"
                            "仍以实际数据为准")
                            .arg(realCrc, 8, 16, QLatin1Char('0')));
                    }
                }
            }
        }

        emit logLine(QStringLiteral(
            "固件: %1 (%2 字节) → 跳过 0x6000 BOOTLOADER 头, 发送 APP %3 字节")
            .arg(QFileInfo(imagePath).fileName())
            .arg(rawSize)
            .arg(image.size()));
    } else {
        image = raw;
        emit logLine(QStringLiteral("固件: %1 (%2 字节) → 整包烧录到 0x08000000")
            .arg(QFileInfo(imagePath).fileName())
            .arg(rawSize));
    }

    /* --- 对齐 --- */
    const int align = info.writeSize > 0 ? static_cast<int>(info.writeSize) : 2;
    const int rem = image.size() % align;
    if (rem != 0)
        image.append(QByteArray(align - rem, '\xFF'));

    if (static_cast<quint32>(image.size()) > hello.slotSize) {
        fail(QStringLiteral("镜像 %1 字节超出设备 slot (%2 字节)")
             .arg(image.size()).arg(hello.slotSize));
        return;
    }

    int usable = qMin<int>(static_cast<int>(hello.maxChunkSize),
                           BOOT_FRAME_MAX_PAYLOAD - BOOT_DATA_PREFIX_SIZE);
    usable -= usable % align;
    if (usable <= 0) {
        fail(QStringLiteral("设备报告的 max_chunk_size 不可用 (%1)").arg(hello.maxChunkSize));
        return;
    }

    const quint32 imageCrc = crc32(image);

    /* --- BEGIN --- */
    QByteArray begin;
    putU32(begin, hello.targetId);
    putU32(begin, info.boardId);
    putU32(begin, info.flashLayoutId);
    putU32(begin, static_cast<quint32>(image.size()));
    putU32(begin, imageCrc);
    putU32(begin, 1u);      /* firmware version */
    putU32(begin, 0u);      /* flags: 裸 BIN, 不校验签名、不加密 */
    putU32(begin, 0u);      /* security version */
    putU32(begin, 0u);      /* min source version */

    QByteArray rsp;
    quint16 seq = 3;
    if (!transact(port, BOOT_OP_BEGIN, seq++, begin, &rsp, 6000, &err)) {
        fail(QStringLiteral("BEGIN 无应答: %1").arg(err));
        return;
    }
    if (rsp.size() < 20 || getU32(rsp.constData() + 4) != 0) {
        fail(QStringLiteral("BEGIN 失败: %1").arg(statusText(rsp)));
        return;
    }
    emit logLine(QStringLiteral("BEGIN ok (%1 字节, crc 0x%2)")
        .arg(image.size())
        .arg(imageCrc, 8, 16, QLatin1Char('0')));

    /* --- DATA --- */
    const int total = (image.size() + usable - 1) / usable;
    int index = 0;

    for (int offset = 0; offset < image.size(); offset += usable) {
        if (m_abort.loadAcquire()) {
            transact(port, BOOT_OP_ABORT, seq++, QByteArray(), &rsp, 1000, nullptr);
            fail(QStringLiteral("已被用户取消"));
            return;
        }

        const QByteArray chunk = image.mid(offset, usable);

        QByteArray payload;
        putU32(payload, static_cast<quint32>(offset));
        putU32(payload, static_cast<quint32>(chunk.size()));
        payload += chunk;

        if (!transact(port, BOOT_OP_DATA, seq++, payload, &rsp, 1500, &err)) {
            fail(QStringLiteral("DATA 0x%1 无应答: %2")
                 .arg(offset, 6, 16, QLatin1Char('0')).arg(err));
            return;
        }
        if (rsp.size() < 20 || getU32(rsp.constData() + 4) != 0) {
            fail(QStringLiteral("DATA 写 0x%1 失败: %2")
                 .arg(offset, 6, 16, QLatin1Char('0')).arg(statusText(rsp)));
            return;
        }

        ++index;
        const quint32 wrote = getU32(rsp.constData() + 8);
        const int pct = static_cast<int>(static_cast<qint64>(wrote) * 100
                                         / image.size());
        emit progress(qBound(0, pct, 99),
                      QStringLiteral("写入 %1/%2 块").arg(index).arg(total));
        emit logLine(QStringLiteral("DATA %1/%2  0x%3  ok  (%4%)")
            .arg(index).arg(total)
            .arg(offset, 6, 16, QLatin1Char('0'))
            .arg(pct));
    }

    /* --- COMMIT --- */
    QByteArray commit;
    putU32(commit, static_cast<quint32>(image.size()));
    putU32(commit, imageCrc);

    if (!transact(port, BOOT_OP_COMMIT, seq++, commit, &rsp, 5000, &err)) {
        fail(QStringLiteral("COMMIT 无应答: %1").arg(err));
        return;
    }

    const quint32 lastErr = rsp.size() >= 20 ? getU32(rsp.constData() + 4) : 0xFFFFFFFFu;
    const quint32 status  = rsp.size() >= 20 ? getU32(rsp.constData() + 0) : 0xFFFFFFFFu;
    if (lastErr != 0 || status != 3) {
        fail(QStringLiteral("COMMIT 失败: %1").arg(statusText(rsp)));
        return;
    }

    emit progress(100, QStringLiteral("烧录完成"));
    emit logLine(QStringLiteral("COMMIT ok — 固件已写入并校验通过"));

    port.close();
    m_busy.storeRelease(0);
    emit flashFinished(true, QString());
}

/* ---------------- 重启读卡器 ---------------- */

void BootFlasher::bootApp(const QString &portName)
{
    m_busy.storeRelease(1);
    m_abort.storeRelease(0);

    QSerialPort port;
    QString err;

    if (!openPort(port, portName, &err)) {
        m_busy.storeRelease(0);
        emit bootAppFinished(false, err);
        return;
    }

    /* BOOT_APP 会让 MCU 立刻复位, 应答可能来不及发回, 超时不算失败 */
    QByteArray rsp;
    const bool answered = transact(port, BOOT_OP_BOOT_APP, 1, QByteArray(),
                                   &rsp, 800, nullptr);

    port.close();
    m_busy.storeRelease(0);

    if (answered && rsp.size() >= 20 && getU32(rsp.constData() + 4) != 0) {
        emit bootAppFinished(false,
            QStringLiteral("设备拒绝启动应用: %1").arg(statusText(rsp)));
        return;
    }

    emit bootAppFinished(true, QString());
}
