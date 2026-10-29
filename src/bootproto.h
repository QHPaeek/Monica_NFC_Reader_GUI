#ifndef BOOTPROTO_H
#define BOOTPROTO_H

/* ====================================================================
 *  Affine Uni Bootloader — 主机端协议实现
 *
 *  与仓库 tools/boot_test.py 完全兼容 (协议版本 0x00010006):
 *      帧 = magic(4) | opcode(2) | sequence(2) | payload_len(4)
 *           | payload_crc32(4) | header_crc32(4) | payload
 *      全部小端; header_crc32 = crc32(前 16 字节)
 *
 *  支持操作: HELLO / GET_DEVICE_INFO / BEGIN / DATA / COMMIT / BOOT_APP
 *  仅实现裸 .bin (无签名 manifest / 无 AES 加密) 的烧录路径。
 *
 *  对象设计为 moveToThread 到独立线程运行 —— 烧录 60KB 需要上百帧
 *  往返, 放在 GUI 线程会卡死界面。
 * ==================================================================== */

#include <QObject>
#include <QByteArray>
#include <QString>
#include <QAtomicInt>

#define BOOT_FRAME_MAGIC        0x42464641u
#define BOOT_PROTOCOL_VERSION   0x00010006u
#define BOOT_FRAME_MAX_PAYLOAD  512
#define BOOT_DATA_PREFIX_SIZE   8

/* 合并固件中 BOOTLOADER 占用的头部长度 (APP 从 0x08006000 开始) */
#define BOOT_IMAGE_HEADER_SIZE  0x6000u
#define BOOT_APP_BASE_ADDR      (0x08000000u + BOOT_IMAGE_HEADER_SIZE)
#define BOOT_FLASH_END_ADDR     0x08020000u   /* STM32F072CB: 128 KiB */

/* 固件镜像形态 —— 决定能不能走 BOOT 通道
 *
 *   Merged   0x0000 是 bootloader 向量表, 0x6000 是 APP 向量表
 *            (v0.8 起的合并固件, 可走 BOOT)
 *   AppOnly  只有 0x0000 处一颗 APP 向量表, 0x6000 处不是向量表
 *            (v0.7 及以下的老固件 / 纯 APP 镜像, 只能走 DFU)
 *   Invalid  文件过小或向量表不合法
 */
enum class BootImageKind {
    Invalid,
    Merged,
    AppOnly,
};

/* 读取文件头并判断镜像形态 (只读前 0x6008 字节, 很快) */
BootImageKind classifyFirmwareImage(const QString &path);

enum BootOp : quint16 {
    BOOT_OP_HELLO           = 0x0001,
    BOOT_OP_BEGIN           = 0x0002,
    BOOT_OP_DATA            = 0x0003,
    BOOT_OP_COMMIT          = 0x0004,
    BOOT_OP_ABORT           = 0x0005,
    BOOT_OP_GET_STATUS      = 0x0006,
    BOOT_OP_BOOT_APP        = 0x0007,
    BOOT_OP_GET_DIAG        = 0x0008,
    BOOT_OP_GET_METADATA    = 0x0009,
    BOOT_OP_CLEAR_METADATA  = 0x000A,
    BOOT_OP_REBOOT          = 0x000B,
    BOOT_OP_VERIFY_IMAGE    = 0x000C,
    BOOT_OP_SET_MANIFEST    = 0x000D,
    BOOT_OP_GET_DEVICE_INFO = 0x000E,
    BOOT_OP_RESPONSE        = 0x8000,
};

/* HELLO 应答 */
struct BootHello {
    quint32 protocolVersion = 0;
    quint32 targetId        = 0;
    quint32 flashSize       = 0;
    quint32 appBase         = 0;
    quint32 slotSize        = 0;
    quint32 maxChunkSize    = 0;
    quint32 capabilities    = 0;
};

/* GET_DEVICE_INFO 应答 (只保留用得到的字段) */
struct BootDeviceInfo {
    quint32 protocolVersion = 0;
    quint32 targetId        = 0;
    quint32 boardId         = 0;
    quint32 flashLayoutId   = 0;
    quint32 appBase         = 0;
    quint32 metadataBase    = 0;   /* metadata 起始地址, APP slot 到此为止 */
    quint32 metadataSize    = 0;
    quint32 writeSize       = 0;
    quint32 maxChunkSize    = 0;
};

/* GET_STATUS / BEGIN / DATA / COMMIT 应答前 5 个字 */
struct BootStatus {
    quint32 status     = 0;
    quint32 lastError  = 0;
    quint32 written    = 0;
    quint32 expected   = 0;
    quint32 runningCrc = 0;
};

class QSerialPort;

class BootFlasher : public QObject
{
    Q_OBJECT

public:
    explicit BootFlasher(QObject *parent = nullptr);

    bool isBusy() const { return m_busy.loadAcquire() != 0; }

public slots:
    /* 只握手: HELLO + DEVICE_INFO, 用于判断串口上是否真是 bootloader */
    void probe(const QString &portName);

    /* 烧录裸 .bin; skipBootHeader=true 时丢弃前 0x6000 字节 */
    void flash(const QString &portName, const QString &imagePath,
               bool skipBootHeader);

    /* 重启读卡器: 发 BOOT_APP 跳回应用 */
    void bootApp(const QString &portName);

    /* HID 未连接时, 通过串口发送 AimeIO 的 JUMP2BOOT (0xFF 0x62),
     * 让应用固件跳进 bootloader。
     * preferredBaud <= 0 时依次尝试 115200 / 38400 / 9600。
     */
    void sendAimeJumpToBoot(const QString &portName, int preferredBaud);

    /* 请求中止当前操作 (GUI 线程调用) */
    void requestAbort() { m_abort.storeRelease(1); }

signals:
    void logLine(const QString &line);
    void progress(int percent, const QString &detail);
    void probeFinished(bool ok, const QString &summary);
    void flashFinished(bool ok, const QString &error);
    void bootAppFinished(bool ok, const QString &error);
    void serialJumpSent(bool ok, const QString &error);

private:
    bool openPort(QSerialPort &port, const QString &name, QString *err);
    bool openPortAt(QSerialPort &port, const QString &name, int baud,
                    bool forceDtr, QString *err);
    bool handshake(QSerialPort &port, BootHello *hello,
                   BootDeviceInfo *info, QString *err);
    bool transact(QSerialPort &port, quint16 opcode, quint16 seq,
                  const QByteArray &payload, QByteArray *response,
                  int timeoutMs, QString *err);
    bool writeFrame(QSerialPort &port, quint16 opcode, quint16 seq,
                    const QByteArray &payload);
    bool readFrame(QSerialPort &port, quint16 expectOpcode,
                   QByteArray *payload, int timeoutMs, QString *err);

    static quint32 crc32(const QByteArray &data);
    static QString statusText(const QByteArray &payload);
    static QString errorName(quint32 code);

    QAtomicInt m_busy;
    QAtomicInt m_abort;
};

#endif // BOOTPROTO_H
