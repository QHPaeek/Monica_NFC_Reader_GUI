#include "aimeserial.h"

#include <QtSerialPort/QSerialPort>
#include <QTimer>
#include <QThread>
#include <cstring>

namespace {

constexpr uint8_t AIME_START   = 0xFF;
constexpr int     FRAME_OUT    = 32;   // PC→设备: [0xFF][cmd][payload 30]
constexpr int     FRAME_IN     = 63;   // 设备→PC: 固件发 raw_data+1, 63 字节

/* 固件 AimeIO 命令字 */
enum : uint8_t {
    CMD_FIND_CARD  = 0x01,
    CMD_WRITE_CARD = 0x02,
    CMD_LED        = 0x60,
    CMD_JUMP2DFU   = 0x61,
    CMD_JUMP2BOOT  = 0x62,
    CMD_LED_BRIGHT = 0x63,
    CMD_FLASH_READ = 0x70,
    CMD_FLASH_WRITE= 0x71,
    CMD_OK         = 0xFD,
    CMD_NO         = 0xFC,
};

bool knownCommand(uint8_t cmd)
{
    switch (cmd) {
    case CMD_FIND_CARD:
    case CMD_WRITE_CARD:
    case CMD_LED:
    case CMD_LED_BRIGHT:
    case CMD_JUMP2DFU:
    case CMD_JUMP2BOOT:
    case CMD_FLASH_READ:
    case CMD_FLASH_WRITE:
    case CMD_OK:
    case CMD_NO:
        return true;
    default:
        return false;
    }
}

} // namespace

AimeSerialWorker::AimeSerialWorker(QObject *parent)
    : QObject(parent)
{
    m_port = new QSerialPort(this);
    connect(m_port, &QSerialPort::readyRead,
            this, &AimeSerialWorker::onReadyRead);

    m_probeTimer = new QTimer(this);
    m_probeTimer->setSingleShot(true);
    connect(m_probeTimer, &QTimer::timeout,
            this, &AimeSerialWorker::onProbeTimeout);
}

AimeSerialWorker::~AimeSerialWorker()
{
    if (m_port && m_port->isOpen())
        m_port->close();
}

int AimeSerialWorker::baudRate() const
{
    return m_port ? m_port->baudRate() : 0;
}

/* --------------------------------------------------------------------
 *  连接：依次尝试波特率，发一条 Flash_Read 作为握手
 * -------------------------------------------------------------------- */

void AimeSerialWorker::connectPort(const QString &portName, int preferredBaud)
{
    disconnectPort();

    m_portName = portName;
    m_buf.clear();
    m_connected = false;
    m_probing   = true;

    m_bauds.clear();
    if (preferredBaud > 0)
        m_bauds << preferredBaud;
    for (int b : {115200, 38400, 9600}) {
        if (!m_bauds.contains(b))
            m_bauds << b;
    }
    m_baudIdx = 0;

    tryNextBaud();
}

void AimeSerialWorker::tryNextBaud()
{
    if (!m_port)
        return;

    if (m_baudIdx >= m_bauds.size()) {
        m_probing = false;
        emit deviceError(QStringLiteral(
            "%1 在 115200 / 38400 / 9600 下都没有 AimeIO 应答").arg(m_portName));
        return;
    }

    if (m_port->isOpen())
        m_port->close();

    m_buf.clear();

    const int baud = m_bauds.at(m_baudIdx);

    m_port->setPortName(m_portName);
    m_port->setBaudRate(baud);
    m_port->setDataBits(QSerialPort::Data8);
    m_port->setParity(QSerialPort::NoParity);
    m_port->setStopBits(QSerialPort::OneStop);
    m_port->setFlowControl(QSerialPort::NoFlowControl);

    if (!m_port->open(QIODevice::ReadWrite)) {
        m_probing = false;
        emit deviceError(QStringLiteral("无法打开 %1: %2")
                             .arg(m_portName, m_port->errorString()));
        return;
    }

    m_port->clear(QSerialPort::AllDirections);

    /* 握手: 读一次配置。收到 0x70 应答即认为通道可用 */
    sendCommand(CMD_FLASH_READ, nullptr, 0);
    m_probeTimer->start(400);
}

void AimeSerialWorker::onProbeTimeout()
{
    if (m_connected)
        return;

    ++m_baudIdx;
    tryNextBaud();
}

void AimeSerialWorker::disconnectPort()
{
    if (m_probeTimer)
        m_probeTimer->stop();

    m_probing = false;
    const bool wasConnected = m_connected;
    m_connected = false;
    m_buf.clear();

    if (m_port && m_port->isOpen())
        m_port->close();

    if (wasConnected)
        emit deviceDisconnected();
}

void AimeSerialWorker::stopWithError(const QString &msg)
{
    disconnectPort();
    emit deviceError(msg);
}

/* --------------------------------------------------------------------
 *  收发
 * -------------------------------------------------------------------- */

void AimeSerialWorker::sendCommand(uint8_t cmd, const uint8_t *payload, int len)
{
    if (!m_port || !m_port->isOpen())
        return;

    QByteArray out(FRAME_OUT, '\0');
    out[0] = static_cast<char>(AIME_START);
    out[1] = static_cast<char>(cmd);

    if (payload && len > 0) {
        const int n = qMin(len, FRAME_OUT - 2);
        memcpy(out.data() + 2, payload, static_cast<size_t>(n));
    }

    emit rawDataSent(out);

    m_port->write(out);
    m_port->waitForBytesWritten(100);
}

void AimeSerialWorker::onReadyRead()
{
    if (!m_port)
        return;

    const QByteArray chunk = m_port->readAll();
    if (chunk.isEmpty())
        return;

    emit rawDataReceived(chunk);
    m_buf += chunk;

    while (m_buf.size() >= FRAME_IN) {
        /* 对齐到 0xFF 帧头 */
        if (static_cast<uint8_t>(m_buf.at(0)) != AIME_START) {
            m_buf.remove(0, 1);
            continue;
        }
        /* 命令字不认识 → 大概率没对齐, 挪一个字节重找 */
        if (!knownCommand(static_cast<uint8_t>(m_buf.at(1)))) {
            m_buf.remove(0, 1);
            continue;
        }

        const QByteArray frame = m_buf.left(FRAME_IN);
        m_buf.remove(0, FRAME_IN);

        if (m_probing) {
            m_probing = false;
            m_probeTimer->stop();
            m_connected = true;
            emit deviceConnected(QStringLiteral("%1 @ %2 bps")
                                     .arg(m_portName).arg(m_port->baudRate()));
        }

        parseFrame(frame);
    }
}

void AimeSerialWorker::parseFrame(const QByteArray &frame)
{
    const uint8_t *d = reinterpret_cast<const uint8_t *>(frame.constData());
    const uint8_t cmd = d[1];

    switch (cmd) {

    case CMD_FIND_CARD: {
        /* 与 HID 完全相同的布局: [0xFF][cmd][type][raw 48] */
        const uint8_t cardType = d[2];
        const QByteArray raw(reinterpret_cast<const char *>(d + 3), 48);
        emit cardDetected(cardType, raw);
        break;
    }

    case CMD_FLASH_READ: {
        /* [0xFF][cmd][readerType][fw][flash 16] */
        const uint8_t readerType = d[2];
        const uint8_t fwVersion  = d[3];

        FlashData flashData;
        memset(&flashData, 0, sizeof(FlashData));
        memcpy(flashData.raw_flash_byte, d + 4, 16);

        emit flashDataRead(flashData, readerType, fwVersion);
        break;
    }

    case CMD_FLASH_WRITE:
        emit flashDataWritten(true);
        break;

    case CMD_WRITE_CARD:
        emit writeCardResult(d[2]);
        break;

    case CMD_JUMP2BOOT:
        emit jumpToBootSent();
        break;

    default:
        break;
    }
}

/* --------------------------------------------------------------------
 *  命令
 * -------------------------------------------------------------------- */

void AimeSerialWorker::requestFlashRead()
{
    sendCommand(CMD_FLASH_READ, nullptr, 0);
}

void AimeSerialWorker::requestFlashWrite(const FlashData &data)
{
    sendCommand(CMD_FLASH_WRITE, data.raw_flash_byte, 16);
}

void AimeSerialWorker::requestLedColor(uint8_t r, uint8_t g, uint8_t b)
{
    uint8_t payload[3] = { r, g, b };
    sendCommand(CMD_LED, payload, 3);
}

void AimeSerialWorker::requestLedBrightness(uint8_t value)
{
    sendCommand(CMD_LED_BRIGHT, &value, 1);
}

void AimeSerialWorker::requestWriteCard(uint8_t sector, uint8_t block,
                                        QByteArray keyA, QByteArray keyB,
                                        QByteArray data)
{
    uint8_t payload[30] = {};
    payload[0] = sector;
    payload[1] = block;
    memcpy(payload + 2,  keyA.constData(),  qMin(keyA.size(),  6));
    memcpy(payload + 8,  keyB.constData(),  qMin(keyB.size(),  6));
    memcpy(payload + 14, data.constData(),  qMin(data.size(), 16));

    sendCommand(CMD_WRITE_CARD, payload, 30);
}

void AimeSerialWorker::requestJumpToBoot()
{
    sendCommand(CMD_JUMP2BOOT, nullptr, 0);
}
