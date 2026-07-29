#ifndef MAINWINDOW_H
#define MAINWINDOW_H

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

#include "flashdata.h"
#include "hidworker.h"

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

    /* 语言切换 */
    void onToggleLanguage();

    /* LED */
    void onLedSliderChanged(int value);

    /* 原始数据日志 */
    void onRawDataSent(const QByteArray &data);
    void onRawDataReceived(const QByteArray &data);

    /* DFU */
    void onJumpToDfu();
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
    void showWriteCardError(uint8_t status);

private:
    /* ---- UI 初始化 ---- */
    void setupUI();
    void applyStyleSheet();
    void applyLanguage();

    QWidget* buildHeaderPanel();
    QWidget* buildCardMonitorPanel();
    QWidget* buildSettingsPanel();
    QWidget* buildDfuPage();
    QWidget* buildWriteCardPage();
    QWidget* createSegaTab();
    QWidget* createSpiceTab();
    QWidget* createNamcoTab();
    QWidget* createSystemTab();

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
                           const QString &cardNumber);
    void clearCardDisplay();
    void appendLogLine(const QString &prefix, const QString &hex,
                       const QString &color);

    /* 辅助 */
    void setControlsEnabled(bool enabled);
    void updateConnectionUI(bool connected);
    QString trC(const QString &zh, const QString &en) const;

    /* 卡种信息 */
    static QString cardTypeName(uint8_t cardType);
    static QString extractUid(uint8_t cardType, const QByteArray &rawData);
    static QString extractCardNumber(uint8_t cardType,
                                     const QByteArray &rawData);

    /* ---- 数据 ---- */
    HIDWorker  *m_hid;
    FlashData   m_flashData;
    bool        m_connected;
    bool        m_langEn;

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

    /* 按钮 */
    QPushButton *m_btnRead;
    QPushButton *m_btnWrite;
    QPushButton *m_btnReset;
    QPushButton *m_btnDfu;

    /* 右侧内容栈 (设置 / DFU / 写卡) */
    QStackedWidget *m_stackedRight;
    QWidget        *m_dfuPage;
    QWidget        *m_writeCardPage;

    /* DFU 页面 */
    QLineEdit      *m_dfuFileEdit;
    QPushButton    *m_dfuBrowseBtn;
    QPushButton    *m_dfuFlashBtn;
    QPlainTextEdit *m_dfuLog;
    QProcess       *m_dfuProcess;
    QString         m_dfuFilePath;    // 固件文件路径 (.bin/.hex/.dfu)
    QString         m_dfuToolPath;    // dfu-util.exe 路径
    bool            m_inDfuMode;

    /* 网络下载 dfu-util */
    QNetworkAccessManager *m_netManager;
    QProgressDialog       *m_dlProgress;
    QPushButton           *m_dfuDownloadBtn;

    /* 状态栏 */
    QLabel     *m_lblStatusMsg;

    /* 写卡 */
    QLineEdit  *m_writeCardInput;
    QLabel     *m_writeCardHint;
    QPushButton *m_writeCardBtn;
    QPushButton *m_writeCardBackBtn;
    QDialog    *m_writeCardDialog;
    QTimer     *m_writeCardTimer;
    bool        m_writeCardPending;
    int         m_writeCardStep;   // 0=block2, 1=block3
    QByteArray  m_writeCardSerial;
    QPushButton *m_btnWriteCardTab;  // 左侧切换到写卡页的按钮
    uint8_t     m_lastCardType;       // 最近一次检测到的卡种
};

#endif // MAINWINDOW_H
