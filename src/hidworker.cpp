#include "hidworker.h"

#include <QDebug>
#include <QString>
#include <QCoreApplication>

/* ---- HIDAPI C header ---- */
extern "C" {
#include <hidapi.h>
}

/* ====================================================================
 *  构造 & 析构
 * ==================================================================== */

HIDWorker::HIDWorker(QObject *parent)
    : QObject(parent)
    , m_thread(new QThread(this))
    , m_running(false)
    , m_connected(false)
    , m_device(nullptr)
{
    hid_init();

    // 将本对象 move 到 worker 线程 (信号发出的槽在该线程执行)
    this->moveToThread(m_thread);
}

HIDWorker::~HIDWorker()
{
    stop();
    hid_exit();
}

/* ---- 公开接口: 启动 / 停止 worker 线程 ---- */

void HIDWorker::start()
{
    if (m_thread->isRunning()) {
        // 已在运行, 先停止再重启
        m_running = false;
        if (m_device) {
            hid_close(m_device);
            m_device = nullptr;
        }
        m_thread->quit();
        m_thread->wait(2000);
    }

    m_thread->start();
    // 在 worker 线程的事件循环中执行 startPolling
    QMetaObject::invokeMethod(this, "startPolling", Qt::QueuedConnection);
}

void HIDWorker::stop()
{
    m_running = false;
    if (m_device) {
        hid_close(static_cast<hid_device*>(m_device));
        m_device = nullptr;
    }
    if (m_connected) {
        m_connected = false;
        emit deviceDisconnected();
    }
    m_thread->quit();
    m_thread->wait(2000);
}

/* ====================================================================
 *  公开槽: 主线程调用 -> 排队到 worker 线程执行
 * ====================================================================
 */

/* 启动设备搜索 (在 worker 线程中) */
void HIDWorker::startPolling()
{
    if (m_running) return;

    // 枚举所有 HID 设备, 按 Usage Page 筛选
    hid_device_info *devs = hid_enumerate(0, 0);
    if (!devs) {
        emit deviceError(QStringLiteral("No HID devices found on this system"));
        return;
    }

    hid_device_info *target = nullptr;
    QStringList candidates;

    for (hid_device_info *cur = devs; cur != nullptr; cur = cur->next) {
        // 记录所有可能的候选
        candidates.append(QStringLiteral("  vid=%1 pid=%2 usage=%3:%4  %5")
            .arg(cur->vendor_id,    4, 16, QLatin1Char('0'))
            .arg(cur->product_id,   4, 16, QLatin1Char('0'))
            .arg(cur->usage_page,   4, 16, QLatin1Char('0'))
            .arg(cur->usage,        4, 16, QLatin1Char('0'))
            .arg(QString::fromWCharArray(cur->product_string)));

        // 匹配 Usage Page 0xAFCA 的设备
        if (cur->usage_page == HID_VENDOR_USAGE_PAGE) {
            target = cur;
            break;  // 取第一个匹配设备
        }
    }

    if (!target) {
        qDebug() << "[HID] No device with Usage Page 0xAFCA found. Candidates:";
        for (const auto &s : candidates) {
            qDebug().noquote() << s;
        }
        emit deviceError(QStringLiteral(
            "NFC Card Reader not found.\n\n"
            "Make sure the device is connected and recognized.\n"
            "(Looking for Usage Page 0xAFCA, Usage 0x02)"));
        hid_free_enumeration(devs);
        return;
    }

    // 提取设备信息
    QString devPath = QString::fromUtf8(target->path);
    QString devDesc = QString::fromWCharArray(target->product_string);
    if (devDesc.isEmpty()) {
        devDesc = QStringLiteral("NFC Card Reader (vid=%1 pid=%2)")
            .arg(target->vendor_id, 4, 16, QLatin1Char('0'))
            .arg(target->product_id, 4, 16, QLatin1Char('0'));
    }

    emit deviceFound(devPath, devDesc);

    // 打开设备
    m_device = hid_open_path(target->path);
    hid_free_enumeration(devs);

    if (!m_device) {
        emit deviceError(QStringLiteral("Failed to open HID device.\n\n"
            "The device may be in use by another application."));
        return;
    }

    // 设置非阻塞以支持带超时的读取
    // hid_set_nonblocking(m_device, 1);  // 用 hid_read_timeout 代替

    m_connected = true;
    emit deviceConnected(devDesc);
    qDebug() << "[HID] Connected:" << devDesc;

    // 进入主读循环
    runLoop();
}

/* 断开设备 */
void HIDWorker::stopPolling()
{
    m_running = false;

    if (m_device) {
        hid_close(m_device);
        m_device = nullptr;
    }

    if (m_connected) {
        m_connected = false;
        emit deviceDisconnected();
    }

    m_thread->quit();
    m_thread->wait(2000);
}

/* ---- 协议命令 ---- */

void HIDWorker::requestFlashRead()
{
    uint8_t payload = 0;  // 无额外数据
    QByteArray report = buildOutputReport(CMD_Flash_Read, &payload, 0);
    writeReport(report);
}

void HIDWorker::requestFlashWrite(const FlashData &data)
{
    // 发送 16 字节 Flash 数据 (固件只写后 7 字节 settings)
    QByteArray report = buildOutputReport(CMD_Flash_Write,
                                          data.raw_flash_byte, 16);
    writeReport(report);
}

void HIDWorker::requestLedColor(uint8_t r, uint8_t g, uint8_t b)
{
    uint8_t payload[3] = { r, g, b };
    QByteArray report = buildOutputReport(CMD_LED, payload, 3);
    writeReport(report);
}

/* 全局亮度 (0x63)。固件在数值变化时会写 Flash，所以不要频繁调用，
 * 由界面上的「写入」按钮触发。
 */
void HIDWorker::requestLedBrightness(uint8_t value)
{
    uint8_t payload = value;
    QByteArray report = buildOutputReport(CMD_LED_Brightness, &payload, 1);
    writeReport(report);
}

void HIDWorker::requestJumpToDfu()
{
    uint8_t payload = 0;
    QByteArray report = buildOutputReport(CMD_JUMP2DFU, &payload, 0);
    writeReport(report);
}

/* 跳转到自研 BOOTLOADER (0x62) — 固件收到后回包随即跳转,
 * 设备会重新枚举为 CDC 串口 (VID AFF1 / PID 52B2)。
 */
void HIDWorker::requestJumpToBoot()
{
    uint8_t payload = 0;
    QByteArray report = buildOutputReport(CMD_JUMP2BOOT, &payload, 0);
    writeReport(report);
}

void HIDWorker::requestWriteCard(uint8_t sector, uint8_t block,
                                  QByteArray keyA, QByteArray keyB,
                                  QByteArray data)
{
    uint8_t payload[30] = {};
    payload[0] = sector;
    payload[1] = block;
    memcpy(payload + 2,  keyA.constData(),  qMin(keyA.size(),  6));
    memcpy(payload + 8,  keyB.constData(),  qMin(keyB.size(),  6));
    memcpy(payload + 14, data.constData(),  qMin(data.size(), 16));

    QByteArray report = buildOutputReport(CMD_Write_Card, payload, 30);
    writeReport(report);
}

/* ====================================================================
 *  组装 Output Report (Report ID 5)
 *
 *  Report 格式 (共 33 字节):
 *    [0]    = Report ID (0x05)
 *    [1]    = AimeIO_Start  (0xFF)
 *    [2]    = Cmd
 *    [3..]  = payload 数据 (不超过 30 字节)
 *    剩余字节补 0
 * ==================================================================== */
QByteArray HIDWorker::buildOutputReport(uint8_t cmd,
                                        const uint8_t *payload,
                                        int payloadLen)
{
    // 32 字节数据 + 1 字节 Report ID = 33 字节
    QByteArray report(HID_REPORT_OUT_SIZE, '\0');

    report[0] = HID_REPORT_ID_OUT;   // 0x05
    report[1] = AimeIO_Start;        // 0xFF
    report[2] = cmd;

    if (payload && payloadLen > 0) {
        int copyLen = qMin(payloadLen, HID_REPORT_OUT_SIZE - 3);
        memcpy(report.data() + 3, payload, static_cast<size_t>(copyLen));
    }

    return report;
}

/* ---- 发送 Output Report (线程安全) ---- */
void HIDWorker::writeReport(const QByteArray &report)
{
    if (!m_device) return;

    emit rawDataSent(report);

    int ret = hid_write(m_device,
                        reinterpret_cast<const unsigned char*>(report.constData()),
                        static_cast<size_t>(report.size()));

    if (ret < 0) {
        emit deviceError(QStringLiteral("Write error: %1")
            .arg(QString::fromWCharArray(hid_error(m_device))));
    }
}

/* ====================================================================
 *  阻塞式 HID 读循环 (在 worker 线程中运行)
 * ==================================================================== */
void HIDWorker::runLoop()
{
    m_running = true;
    uint8_t buf[HID_READ_BUF_SIZE];

    while (m_running && m_device) {
        // 200ms 超时 — 短超时让事件循环能及时处理排队的槽 (如 requestFlashRead)
        int ret = hid_read_timeout(m_device, buf, sizeof(buf), 200);

        if (ret > 0) {
            emit rawDataReceived(
                QByteArray(reinterpret_cast<const char*>(buf), ret));
            parseInputReport(buf, ret);
        } else if (ret < 0) {
            // 读取错误 → 设备断开
            qDebug() << "[HID] Read error, device disconnected";
            break;
        }
        // 处理排队的事件 (允许 requestFlashRead 等槽执行)
        QCoreApplication::processEvents();
    }

    // 清理
    if (m_device) {
        hid_close(m_device);
        m_device = nullptr;
    }
    m_connected = false;
    m_running = false;
    emit deviceDisconnected();
}

/* ====================================================================
 *  解析 HID 输入数据
 *
 *  HID Report ID 在 buf[0] (可能是 4 或任意值 — 直接跳过),
 *  AimeIO 协议数据从 buf[1] 开始, 首字节必须是 0xFF。
 * ==================================================================== */
void HIDWorker::parseInputReport(const uint8_t *buf, int len)
{
    if (len < 4) return;           // 至少: ReportID + 0xFF + Cmd + 1 字节

    // buf[0] = HID Report ID — 跳过
    // buf[1] = 必须是 AimeIO_Start (0xFF)
    if (buf[1] != AimeIO_Start) return;

    const uint8_t *data = buf + 1;  // 从 0xFF 开始
    int dataLen = len - 1;
    uint8_t cmd = data[1];

    switch (cmd) {

    case CMD_Find_Card: {
        /* data[2]     = Card_Type
         * data[3..50] = Card_RawData[3][16]  (48 bytes)
         */
        if (dataLen < 3) return;
        uint8_t cardType = data[2];
        int rawStart = 3;
        int rawLen   = qMin(dataLen - rawStart, 48);
        QByteArray rawData(reinterpret_cast<const char*>(data + rawStart), rawLen);

        emit cardDetected(cardType, rawData);
        break;
    }

    case CMD_Flash_Read: {
        /* data[2]      = Reader_Type
         * data[3]      = Firmware_Vision
         * data[4..19]  = Flash_Data[16]
         */
        if (dataLen < 20) return;
        uint8_t readerType = data[2];
        uint8_t fwVersion  = data[3];

        FlashData flashData;
        memset(&flashData, 0, sizeof(FlashData));
        memcpy(flashData.raw_flash_byte, data + 4, 16);

        emit flashDataRead(flashData, readerType, fwVersion);
        break;
    }

    case CMD_Flash_Write: {
        emit flashDataWritten(true);
        break;
    }

    case CMD_Write_Card:
        // data[2] = status (协议定义的多种状态)
        emit writeCardResult(dataLen >= 3 ? data[2] : 0xFF);
        break;
    case CMD_JUMP2DFU:
        // 确认包，无需额外处理
        break;

    default:
        qDebug() << "[HID] Unknown command:" << Qt::hex << cmd;
        break;
    }
}
