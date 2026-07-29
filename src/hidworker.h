#ifndef HIDWORKER_H
#define HIDWORKER_H

#include <QObject>
#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <QByteArray>

#include "flashdata.h"

/* 前向声明 HIDAPI 类型 (避免头文件污染) */
extern "C" {
struct hid_device_;
}

/* --- HID 设备匹配参数 ---
 * 通过 HID Report Descriptor 中的 Usage Page + Usage 唯一识别读卡器:
 *   Usage Page: 0xAFCA
 *   Usage:      0x02
 */
#define HID_VENDOR_USAGE_PAGE  0xAFCA
#define HID_VENDOR_USAGE       0x0002

#define HID_REPORT_ID_OUT      0x05

#define HID_READ_BUF_SIZE      128    // 读缓冲区 (足够容纳任何 HID 报告)
#define HID_REPORT_OUT_SIZE    33     // 1 (Report ID) + 32 数据


/* --- AimeIO 协议常量 --- */
enum AimeIOCmd : uint8_t {
    AimeIO_Start       = 0xFF,

    CMD_Find_Card      = 0x01,
    CMD_Write_Card     = 0x02,
    CMD_LED            = 0x60,
    CMD_JUMP2DFU       = 0x61,
    CMD_Flash_Read     = 0x70,
    CMD_Flash_Write    = 0x71,
};

enum AimeIOCardType : uint8_t {
    Card_None          = 0x00,
    Card_Aime          = 0x01,
    Card_Bana_New      = 0x02,
    Card_Bana_Old      = 0x03,
    Card_AIC           = 0x04,
    Card_EPass         = 0x05,
    Card_Nesica        = 0x06,
    Card_Tunion        = 0x07,
    Card_Jubeat        = 0x08,
    Card_Unknow_Mifare = 0x09,
    Card_Blank_Mifare  = 0x10,
};

/* 读卡器硬件型号 (Monica 系列) */
enum ReaderModel : uint8_t {
    Reader_OpenSource = 0xA1,
    Reader_Lite       = 0xA2,
    Reader_Standard   = 0xA3,
    Reader_Pro        = 0xA4,
};


/* HIDWorker — 基于 HIDAPI 的读卡器通信类
 *
 * 设计:
 *   - 在独立 QThread 中执行阻塞式 HID 读循环
 *   - 通过信号槽与主线程 GUI 解耦
 *   - 所有 HID I/O 操作都是异步的 (写操作推送到线程)
 */
class HIDWorker : public QObject
{
    Q_OBJECT

public:
    explicit HIDWorker(QObject *parent = nullptr);
    ~HIDWorker() override;

    bool isConnected() const { return m_connected; }

public:
    /* 启动 worker 线程 (主线程调用) */
    void start();
    void stop();

public slots:
    /* 启动设备搜索 & 读循环 (在 worker 线程中执行) */
    void startPolling();

    /* 断开设备 & 停止读循环 */
    void stopPolling();

    /* 协议命令 (异步, 非线程安全 — 由主线程调用, 内部排队) */
    void requestFlashRead();
    void requestFlashWrite(const FlashData &data);
    void requestLedColor(uint8_t r, uint8_t g, uint8_t b);
    void requestJumpToDfu();

signals:
    /* 设备／连接状态 */
    void deviceFound(const QString &path, const QString &desc);
    void deviceConnected(const QString &desc);
    void deviceDisconnected();
    void deviceError(const QString &message);

    /* 卡片检测 (来自 SMT32 自主上报的 Find_Card) */
    void cardDetected(uint8_t cardType, const QByteArray &rawData);

    /* Flash 操作响应 */
    void flashDataRead(const FlashData &data, uint8_t readerType,
                       uint8_t fwVersion);
    void flashDataWritten(bool success);

    /* 原始数据监控 — 不做协议解析, 供 GUI 日志窗口使用 */
    void rawDataSent(const QByteArray &report);
    void rawDataReceived(const QByteArray &data);

private:
    /* 线程入口 — 阻塞式 HID 读循环 */
    void runLoop();

    /* HID 写操作 (线程安全, 由主线程通过信号触发) */
    void writeReport(const QByteArray &report);

    /* 解析 HID 输入数据 — 以 0xFF 协议头定位 (不依赖 Report ID) */
    void parseInputReport(const uint8_t *data, int len);

    /* 组装 32 字节 Output Report */
    static QByteArray buildOutputReport(uint8_t cmd,
                                        const uint8_t *payload, int payloadLen);

    /* ---- 数据 ---- */
    QThread   *m_thread;
    QMutex     m_mutex;
    bool       m_running;
    bool       m_connected;

    /* HIDAPI 设备指针 (仅在 worker 线程中访问) */
    hid_device_ *m_device;
};

#endif // HIDWORKER_H
