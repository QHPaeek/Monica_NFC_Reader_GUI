#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#define APP_VERSION  "V1.0.5"
#define APP_VERSION_NUM 0x00010005  // major.minor.patch

/* 连接后读取 Flash 的超时时间 (ms) — 超时视为读卡器死机 */
#define FLASH_READ_TIMEOUT_MS  3000

#include <QMainWindow>

#include <QCheckBox>
#include <QComboBox>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QSplitter>
#include <QStatusBar>
#include <QTabWidget>
#include <QTextEdit>
#include <QTimer>
#include <QVBoxLayout>
#include <QProcess>
#include <QByteArray>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QStackedWidget>
#include <QFileDialog>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QProgressDialog>
#include <QProgressBar>
#include <QMessageBox>
#include <QThread>

#include "flashdata.h"
#include "hidworker.h"
#include "banapasscard.h"
#include "portscan.h"
#include "bootproto.h"
#include "aimeserial.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void onConnectClicked();
    void onDisconnectClicked();
    void onDeviceConnected(const QString &desc);
    void onDeviceDisconnected();
    void onDeviceError(const QString &msg);

    /* 卡片检测 */
    void onCardDetected(uint8_t cardType, const QByteArray &rawData);

    /* Flash 读写 */
    void onReadSettings();
    void onWriteSettings();
    void onResetDefaults();
    void onFlashDataRead(const FlashData &data, uint8_t readerType,
                         uint8_t fwVersion);
    void onFlashDataWritten(bool ok);
    void onFlashReadTimeout();      /* 读取 Flash 超时 → 判定读卡器死机 */

    /* 语言切换 */
    void onToggleLanguage();

    /* LED */
    void onLedSliderChanged(int value);

    /* 原始数据日志 */
    void onRawDataSent(const QByteArray &data);
    void onRawDataReceived(const QByteArray &data);

    /* 固件升级 (BOOTLOADER / DFU) */
    void onFirmwareUpgradeClicked();
    void onEnterBootloader();
    void onEnterDfu();
    void onBootFlash();
    void onBootBrowse();
    void onBootLog(const QString &line);
    void onBootProgress(int percent, const QString &detail);
    void onBootFlashFinished(bool ok, const QString &error);
    void onSerialJumpSent(bool ok, const QString &error);
    void onRebootReader();
    void onRebootFinished(bool ok, const QString &error);
    void onProbeFinished(bool ok, const QString &summary);
    void pollEnterResult();

    /* 串口 AimeIO 通道 */
    void onSerialConnected(const QString &desc);
    void onSerialDisconnected();
    void onSerialError(const QString &msg);

    /* DFU (老固件, dfu-util) */
    void onDfuFileBrowse();
    void onDfuFlash();
    void onDfuListFinished(int exitCode, QProcess::ExitStatus status);
    void onDfuFlashFinished(int exitCode, QProcess::ExitStatus status);
    void onDfuResetFinished(int exitCode, QProcess::ExitStatus status);
    void onDfuCheckTool();
    void tryDownloadDfuUtil();
    void onDfuDownloadFinished(QNetworkReply *reply);

    /* 写 Aime 卡 */
    void onWriteCardClicked();
    void onWriteCardCancel();
    void onWriteCardResult(uint8_t status);
    void onWriteCardTimeout();
    void doWriteBlock2();
    void doWriteBlock3();
    void doWriteAdv();
    void showWriteCardError(uint8_t status);
    void showWriteCardAdvError(uint8_t status);

private:
    /* ---- UI 初始化 ---- */
    void setupUI();
    void applyStyleSheet();
    void applyLanguage();

    QWidget* buildHeaderPanel();
    QWidget* buildCardMonitorPanel();
    QWidget* buildSettingsPanel();
    QWidget* buildFirmwarePage();
    QWidget* buildWriteCardPage();
    QWidget* createSegaTab();
    QWidget* createSpiceTab();
    QWidget* createNamcoTab();
    QWidget* createSystemTab();
    QWidget* buildNormalTab();
    QWidget* buildAdvTab();

    /* Bit ↔ Checkbox */
    void    bindCheckboxesToBits(uint8_t byteVal,
                                 const QCheckBox *const *cbs, int count);
    uint8_t collectBitsFromCheckboxes(
                const QCheckBox *const *cbs, int count) const;

    /* FlashData ↔ UI */
    void     populateSettingsUI(const FlashData &data);
    FlashData collectSettingsFromUI() const;

    /* 卡片显示 */
    void showCardInDisplay(const QString &uid, const QString &cardType,
                           const QString &cardNumber,
                           const QString &cardNumber2 = QString());
    void clearCardDisplay();
    void appendLogLine(const QString &prefix, const QString &hex,
                       const QString &color);

    /* 固件升级辅助 */
    void    updateFirmwareEntryState();
    void    refreshPortCombo();
    bool    bootloaderPort(ComPortInfo *out) const;
    bool    bootloaderConnected(ComPortInfo *out) const;
    bool    dfuDevicePresent() const;
    void    showEnterDialog(int mode, const QString &text);
    void    finishEnterSuccess();
    void    scheduleSerialProbe(int delayMs);
    int     configuredBaud() const;

    /* 顶部设备下拉框的当前选择 */
    QString selectedSerialPort() const;   // 选的是串口时返回 COM 名, 否则空
    QString selectedHidPath() const;
    bool    selectedIsHid() const;

    /* 传输通道抽象: HID 与串口共用同一套 AimeIO 命令 */
    enum Transport { TransportNone, TransportHid, TransportSerial };
    void handleTransportConnected(const QString &desc, bool needFlashRead);
    void sendFlashRead();
    void sendFlashWrite(const FlashData &data);
    void sendLedRgb(uint8_t r, uint8_t g, uint8_t b);
    void sendLedBrightness(uint8_t value);
    void sendWriteCard(uint8_t sector, uint8_t block,
                       QByteArray keyA, QByteArray keyB, QByteArray data);

    static QString idmToDecimal(const QByteArray &idm);

    /* 辅助 */
    void setControlsEnabled(bool enabled);
    void updateConnectionUI(bool connected);
    QString trC(const QString &zh, const QString &en) const;

    /* Flash 读取超时看门狗 */
    void armFlashReadTimeout();
    void disarmFlashReadTimeout();

    /* 卡种信息 */
    static QString cardTypeName(uint8_t cardType);
    static QString extractUid(uint8_t cardType, const QByteArray &rawData);
    static QString extractCardNumber(uint8_t cardType,
                                     const QByteArray &rawData);

    /* ---- 数据 ---- */
    HIDWorker  *m_hid;
    AimeSerialWorker *m_serial;        /* 串口 AimeIO 通道 */
    Transport   m_transport;           /* 当前走 HID 还是串口 */
    bool        m_serialConnecting;    /* bootloader 探测失败后正在连 AimeIO */
    FlashData   m_flashData;
    bool        m_connected;
    bool        m_langEn;
    QTimer     *m_flashReadTimer;   /* Flash 读取超时看门狗 */

    /* ---- Header ---- */
    QLabel     *m_lblConnStatus;
    QLabel     *m_lblFwVersion;
    QLabel     *m_lblConnDot;
    QLabel     *m_lblReaderModel;
    QPushButton *m_btnConnect;
    QPushButton *m_btnDisconnect;
    QPushButton *m_btnLang;

    /* ---- 卡片监控 ---- */
    QFrame     *m_cardDisplayFrame;
    QLabel     *m_lblCardUid;
    QLabel     *m_lblCardNumber;
    QLabel     *m_lblCardNumber2;   /* AIC 第二排: IDM 十进制 */
    QLabel     *m_lblCardType;

    /* ---- 原始数据日志 ---- */
    QTextEdit  *m_rawLog;

    /* ---- 选项卡 ---- */
    QTabWidget *m_tabSettings;

    /* SEGA */
    QCheckBox  *m_sega_tn32;       // BIT0
    QCheckBox  *m_sega_aic;        // BIT1
    QCheckBox  *m_sega_nesica;     // BIT3
    QCheckBox  *m_sega_ea_pass;    // BIT4
    QCheckBox  *m_sega_tunion;     // BIT5
    QCheckBox  *m_sega_jubeat;     // BIT6

    /* SPICE */
    QCheckBox  *m_spice_2p;        // BIT0
    QCheckBox  *m_spice_aime;      // BIT1
    QCheckBox  *m_spice_banapass;  // BIT2
    QCheckBox  *m_spice_nesica;    // BIT3
    QCheckBox  *m_spice_tunion;    // BIT5
    QCheckBox  *m_spice_jubeat;    // BIT6

    /* NAMCO */
    QCheckBox  *m_namco_todo;      // BIT0
    QCheckBox  *m_namco_aime;      // BIT1
    QCheckBox  *m_namco_banapass;  // BIT2
    QCheckBox  *m_namco_nesica;    // BIT3
    QCheckBox  *m_namco_tunion;    // BIT5
    QCheckBox  *m_namco_jubeat;    // BIT6

    /* System */
    QComboBox  *m_cmbUartBaud;
    QSlider    *m_sldLedBright;
    QLabel     *m_lblLedValue;
    QSlider    *m_sldLedR;
    QSlider    *m_sldLedG;
    QSlider    *m_sldLedB;
    QLabel     *m_lblLedR;
    QLabel     *m_lblLedG;
    QLabel     *m_lblLedB;

    /* 按钮 */
    QPushButton *m_btnRead;
    QPushButton *m_btnWrite;
    QPushButton *m_btnReset;
    QPushButton *m_btnDfu;

    /* 右侧内容栈 (设置 / DFU / 写卡) */
    QStackedWidget *m_stackedRight;
    QWidget        *m_dfuPage;
    QWidget        *m_writeCardPage;

    /* 固件升级页 — BOOTLOADER 区 */
    QPushButton    *m_bootEnterBtn;
    QComboBox      *m_bootPortCombo;
    QLabel         *m_bootStateLabel;
    QLineEdit      *m_bootFileEdit;
    QPushButton    *m_bootBrowseBtn;
    QPushButton    *m_bootFlashBtn;
    QProgressBar   *m_bootProgress;
    QString         m_bootFilePath;

    /* 固件升级页 — DFU 区 */
    QLineEdit      *m_dfuFileEdit;
    QPushButton    *m_dfuBrowseBtn;
    QPushButton    *m_dfuFlashBtn;
    QPushButton    *m_dfuEnterBtn;
    QLabel         *m_dfuStateLabel;
    QPlainTextEdit *m_dfuLog;
    QProcess       *m_dfuProcess;
    QString         m_dfuFilePath;    // 固件文件路径 (.bin/.hex/.dfu)
    QString         m_dfuToolPath;    // dfu-util.exe 路径
    bool            m_inDfuMode;

    /* 固件升级页 — 公共 */
    QPushButton    *m_rebootBtn;
    QTimer         *m_portTimer;      // 每秒刷新串口列表
    QTimer         *m_enterTimer;     // 进入 BOOT/DFU 后等待枚举
    QMessageBox    *m_enterDlg;       // 「正在尝试进入…」弹窗
    int             m_enterMode;      // 0=无 1=BOOT 2=DFU
    int             m_enterWaitedMs;  // 已等待毫秒
    bool            m_enterNotified;  // 1 秒提示是否已给出
    bool            m_suppressPortSignal;
    QString         m_portSignature;  // 串口/设备列表比对签名
    QString         m_bootVerifiedPort;   // 已通过协议握手确认的 bootloader 串口
    bool            m_connectProbePending; // 「连接」按钮触发的探测等待中
    QString         m_probePort;           // 当前正在探测的串口
    int             m_probeRetry = 0;      // 进入 BOOT 后的握手重试次数

    /* BOOTLOADER 烧录线程 */
    QThread        *m_bootThread;
    BootFlasher    *m_bootFlasher;
    bool            m_bootBusy;

    /* 网络下载 dfu-util */
    QNetworkAccessManager *m_netManager;
    QProgressDialog       *m_dlProgress;
    QPushButton           *m_dfuDownloadBtn;

    /* 状态栏 */
    QLabel     *m_lblStatusMsg;
    QLabel     *m_lblVersion;

    /* 写卡 */
    QLineEdit  *m_writeCardInput;
    QLabel     *m_writeCardHint;
    QPushButton *m_writeCardBtn;
    QPushButton *m_writeCardBackBtn;
    QPushButton *m_advWriteBtn;
    QDialog    *m_writeCardDialog;
    QTimer     *m_writeCardTimer;
    bool        m_writeCardPending;
    int         m_writeCardStep;   // 0=block2, 1=block3
    QByteArray  m_writeCardSerial;
    uint8_t     m_fwVersion;       // 固件版本 (高4位=主, 低4位=次)
    QTabWidget *m_writeCardTabs;   // 写卡子标签页: 普通 | 高级
    QLineEdit  *m_advSector;
    QLineEdit  *m_advBlock;
    QLineEdit  *m_advKeyA;
    QLineEdit  *m_advKeyB;
    QLineEdit  *m_advData;
    QLabel     *m_advHint;
    QPushButton *m_btnWriteCardTab;  // 左侧切换到写卡页的按钮
    uint8_t     m_lastCardType;       // 最近一次检测到的卡种
};

#endif // MAINWINDOW_H
