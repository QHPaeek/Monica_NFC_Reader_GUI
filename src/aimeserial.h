#ifndef AIMESERIAL_H
#define AIMESERIAL_H

/* ====================================================================
 *  AimeIO over Serial (UART / CDC)
 *
 *  与 HID 通道使用同一套 AimeIO 协议，区别只在报文封装：
 *      HID    PC→设备: [0x05][0xFF][cmd][payload …]   33 字节
 *      Serial PC→设备: [0xFF][cmd][payload …]         32 字节
 *      Serial 设备→PC: [0xFF][cmd][payload …]         固定 63 字节
 *                      (固件 AimeIO_Transmit() 发 raw_data+1, 63)
 *
 *  固件侧路径:
 *      USART1 空闲中断 → Reader_UART_IRQHandler → Packet_process
 *        → Mode_Detect → AimeIO_packet_check(0xFF) → AimeIO_process(cmd)
 *  发过任意一条命令后 Reader.Current_Mode 变 MODE_AIME_IO、
 *  接口锁定 UART，之后卡片检测会主动推到该串口。
 *
 *  设计: 直接在 GUI 线程用 readyRead 信号处理，数据量很小 (63 字节/帧)，
 *        不另开线程，避免 QSerialPort 跨线程访问问题。
 * ==================================================================== */

#include <QObject>
#include <QByteArray>
#include <QString>
#include <QList>

#include "flashdata.h"

class QSerialPort;
class QTimer;

class AimeSerialWorker : public QObject
{
    Q_OBJECT

public:
    explicit AimeSerialWorker(QObject *parent = nullptr);
    ~AimeSerialWorker() override;

    bool isConnected() const { return m_connected; }
    QString portName() const { return m_portName; }
    int baudRate() const;

public slots:
    /* 连接并自动试探波特率 (preferredBaud<=0 时试 115200/38400/9600) */
    void connectPort(const QString &portName, int preferredBaud);
    void disconnectPort();

    void requestFlashRead();
    void requestFlashWrite(const FlashData &data);
    void requestLedColor(uint8_t r, uint8_t g, uint8_t b);
    void requestWriteCard(uint8_t sector, uint8_t block,
                          QByteArray keyA, QByteArray keyB, QByteArray data);

    /* 串口通道下发 AimeIO JUMP2BOOT (0xFF 0x62) */
    void requestJumpToBoot();

    /* 注意: 串口通道**不提供** JUMP2DFU —— 用户明确要求禁止 */

signals:
    void deviceConnected(const QString &desc);
    void deviceDisconnected();
    void deviceError(const QString &message);
    void cardDetected(uint8_t cardType, const QByteArray &rawData);
    void flashDataRead(const FlashData &data, uint8_t readerType, uint8_t fwVersion);
    void flashDataWritten(bool success);
    void writeCardResult(uint8_t status);
    void rawDataSent(const QByteArray &report);
    void rawDataReceived(const QByteArray &data);
    void jumpToBootSent();

private slots:
    void onReadyRead();
    void onProbeTimeout();

private:
    void tryNextBaud();
    void sendCommand(uint8_t cmd, const uint8_t *payload, int len);
    void parseFrame(const QByteArray &frame);
    void stopWithError(const QString &msg);

    QSerialPort *m_port = nullptr;
    QTimer      *m_probeTimer = nullptr;
    QString      m_portName;
    QList<int>   m_bauds;
    int          m_baudIdx = 0;
    bool         m_probing = false;
    bool         m_connected = false;
    QByteArray   m_buf;
};

#endif // AIMESERIAL_H
