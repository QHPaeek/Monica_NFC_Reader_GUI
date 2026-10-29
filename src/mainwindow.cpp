#include "mainwindow.h"

#include <QtEndian>
#include <algorithm>

#include <QApplication>
#include <QDateTime>
#include <QHeaderView>
#include <QMessageBox>
#include <QScrollArea>
#include <QScrollBar>
#include <QDesktopServices>
#include <QUrl>
#include <QStyle>
#include <QThread>
#include <QStandardPaths>
#include <QDir>

/* AIC 解密 / 卡号转换 — 来自固件 accesscode.h */
#include "accesscode.h"

static QString formatCardNumber(const QString &hex)
{
    QString fmt;
    for (int i = 0; i < hex.length(); ++i) {
        if (i > 0 && i % 4 == 0)
            fmt.append(QLatin1Char(' '));
        fmt.append(hex.at(i));
    }
    return fmt;
}

/* UID → 虚拟卡号 (hex_to_accesscode, 来自 STM32 固件)
 * 取 8 字节 UID, 转换成 10 字节 access code, 20 位 HEX 显示.
 */
static QString uidToAccessCode(const uint8_t *uid8)
{
    uint8_t ac[10];
    ac[0] = 0x00;
    ac[1] = 0x00;
    memcpy(ac + 2, uid8, 8);
    for (int i = 0; i < 8; ++i) {
        uint8_t hi = (uid8[i] >> 4);
        uint8_t lo = (uid8[i] & 0x0F);
        if (hi >= 10) hi -= 7;
        if (lo >= 10) lo -= 7;
        ac[i + 2] = (hi << 4) | lo;
    }
    QString hex;
    for (int i = 0; i < 10; ++i)
        hex.append(QStringLiteral("%1").arg(ac[i], 2, 16, QLatin1Char('0')));
    return formatCardNumber(hex.toUpper());
}

// ====================================================================
//  构造 & 析构
// ====================================================================

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_hid(new HIDWorker(nullptr))
    , m_connected(false)
    , m_langEn(false)
    , m_flashReadTimer(nullptr)
    , m_inDfuMode(false)
    , m_netManager(new QNetworkAccessManager(this))
    , m_dlProgress(nullptr)
    , m_dfuDownloadBtn(nullptr)
    , m_writeCardInput(nullptr)
    , m_writeCardHint(nullptr)
    , m_writeCardBtn(nullptr)
    , m_writeCardBackBtn(nullptr)
    , m_writeCardDialog(nullptr)
    , m_writeCardTimer(nullptr)
    , m_writeCardPending(false)
    , m_writeCardStep(0)
    , m_fwVersion(0)
    , m_writeCardTabs(nullptr)
    , m_advWriteBtn(nullptr)
    , m_advSector(nullptr)
    , m_advBlock(nullptr)
    , m_advKeyA(nullptr)
    , m_advKeyB(nullptr)
    , m_advData(nullptr)
    , m_advHint(nullptr)
    , m_btnWriteCardTab(nullptr)
    , m_lastCardType(Card_None)
    , m_bootEnterBtn(nullptr)
    , m_bootPortCombo(nullptr)
    , m_bootPortRefreshBtn(nullptr)
    , m_bootStateLabel(nullptr)
    , m_bootFileEdit(nullptr)
    , m_bootBrowseBtn(nullptr)
    , m_bootFlashBtn(nullptr)
    , m_bootProgress(nullptr)
    , m_dfuEnterBtn(nullptr)
    , m_dfuStateLabel(nullptr)
    , m_rebootBtn(nullptr)
    , m_portTimer(nullptr)
    , m_enterTimer(nullptr)
    , m_enterDlg(nullptr)
    , m_enterMode(0)
    , m_enterWaitedMs(0)
    , m_enterNotified(false)
    , m_suppressPortSignal(false)
    , m_bootThread(nullptr)
    , m_bootFlasher(nullptr)
    , m_bootBusy(false)
{
    memset(&m_flashData, 0, sizeof(FlashData));

    setupUI();
    applyStyleSheet();
    applyLanguage();      // 初始设置中文标签

    /* ---- HIDWorker 信号 ---- */
    connect(m_hid, &HIDWorker::deviceConnected,
            this, &MainWindow::onDeviceConnected);
    connect(m_hid, &HIDWorker::deviceDisconnected,
            this, &MainWindow::onDeviceDisconnected);
    connect(m_hid, &HIDWorker::deviceError,
            this, &MainWindow::onDeviceError);
    connect(m_hid, &HIDWorker::cardDetected,
            this, &MainWindow::onCardDetected);
    connect(m_hid, &HIDWorker::flashDataRead,
            this, &MainWindow::onFlashDataRead);
    connect(m_hid, &HIDWorker::flashDataWritten,
            this, &MainWindow::onFlashDataWritten);
    connect(m_hid, &HIDWorker::writeCardResult,
            this, &MainWindow::onWriteCardResult);
    connect(m_hid, &HIDWorker::rawDataSent,
            this, &MainWindow::onRawDataSent);
    connect(m_hid, &HIDWorker::rawDataReceived,
            this, &MainWindow::onRawDataReceived);

    setControlsEnabled(false);
    updateConnectionUI(false);

    /* ---- BOOTLOADER 烧录线程 (串口 I/O 不能放在 GUI 线程) ---- */
    m_bootThread  = new QThread(this);
    m_bootFlasher = new BootFlasher();
    m_bootFlasher->moveToThread(m_bootThread);

    connect(m_bootThread, &QThread::finished,
            m_bootFlasher, &QObject::deleteLater);
    connect(m_bootFlasher, &BootFlasher::logLine,
            this, &MainWindow::onBootLog);
    connect(m_bootFlasher, &BootFlasher::progress,
            this, &MainWindow::onBootProgress);
    connect(m_bootFlasher, &BootFlasher::flashFinished,
            this, &MainWindow::onBootFlashFinished);
    connect(m_bootFlasher, &BootFlasher::bootAppFinished,
            this, &MainWindow::onRebootFinished);
    connect(m_bootFlasher, &BootFlasher::probeFinished,
            this, &MainWindow::onProbeFinished);

    m_bootThread->start();
}

MainWindow::~MainWindow()
{
    if (m_bootFlasher)
        m_bootFlasher->requestAbort();

    if (m_bootThread) {
        m_bootThread->quit();
        m_bootThread->wait(3000);
    }

    m_hid->stop();
    delete m_hid;
    m_hid = nullptr;
}

/* ---- 中/英文切换辅助 ---- */
QString MainWindow::trC(const QString &zh, const QString &en) const
{
    return m_langEn ? en : zh;
}

// ====================================================================
//  UI 构建
// ====================================================================

void MainWindow::setupUI()
{
    setWindowTitle(QStringLiteral("Monica NFC Reader GUI"));
    resize(1100, 750);
    setMinimumSize(900, 600);

    QWidget *central = new QWidget(this);
    setCentralWidget(central);
    QVBoxLayout *mainLayout = new QVBoxLayout(central);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    mainLayout->addWidget(buildHeaderPanel());

    QSplitter *splitter = new QSplitter(Qt::Horizontal, central);
    splitter->setHandleWidth(1);
    splitter->setChildrenCollapsible(false);
    splitter->addWidget(buildCardMonitorPanel());
    m_stackedRight = new QStackedWidget(central);
    m_stackedRight->addWidget(buildSettingsPanel());   // index 0
    m_stackedRight->addWidget(buildFirmwarePage());     // index 1
    m_stackedRight->addWidget(buildWriteCardPage());    // index 2
    m_stackedRight->setCurrentIndex(0);
    splitter->addWidget(m_stackedRight);
    splitter->setStretchFactor(0, 2);
    splitter->setStretchFactor(1, 3);
    splitter->setSizes({420, 680});
    mainLayout->addWidget(splitter, 1);

    m_lblStatusMsg = new QLabel(QStringLiteral("Ready - Click Connect to start"));
    m_lblStatusMsg->setObjectName(QStringLiteral("statusMsg"));
    statusBar()->addWidget(m_lblStatusMsg, 1);
    m_lblVersion = new QLabel(QStringLiteral(APP_VERSION));
    m_lblVersion->setObjectName(QStringLiteral("statusVersion"));
    statusBar()->addPermanentWidget(m_lblVersion);
    statusBar()->setObjectName(QStringLiteral("appStatusBar"));
}

// ====================================================================
//  Header
// ====================================================================

QWidget* MainWindow::buildHeaderPanel()
{
    QFrame *header = new QFrame(this);
    header->setObjectName(QStringLiteral("headerPanel"));
    header->setFixedHeight(56);

    QHBoxLayout *hbox = new QHBoxLayout(header);
    hbox->setContentsMargins(20, 0, 20, 0);

    QLabel *title = new QLabel(QStringLiteral("NFC Card Reader"));
    title->setObjectName(QStringLiteral("appTitle"));

    QPushButton *btnTutorial = new QPushButton(
        QString::fromUtf8("教程网站"), header);
    btnTutorial->setObjectName(QStringLiteral("btnTutorial"));
    btnTutorial->setCursor(Qt::PointingHandCursor);
    connect(btnTutorial, &QPushButton::clicked, this, []() {
        QDesktopServices::openUrl(QUrl(
            QStringLiteral("https://affinelab.notion.site/"
            "Aime-Manual-66c171d3fc144ae2bc4369d45000f0eb")));
    });

    m_lblConnDot = new QLabel(this);
    m_lblConnDot->setObjectName(QStringLiteral("connDot"));
    m_lblConnDot->setFixedSize(10, 10);

    m_lblConnStatus = new QLabel(QStringLiteral("Disconnected"));
    m_lblConnStatus->setObjectName(QStringLiteral("connStatus"));

    m_lblFwVersion   = new QLabel(QStringLiteral("FW: --"));
    m_lblFwVersion->setObjectName(QStringLiteral("fwVersion"));

    m_lblReaderModel = new QLabel(this);
    m_lblReaderModel->setObjectName(QStringLiteral("readerModel"));
    m_lblReaderModel->hide();

    m_btnConnect    = new QPushButton(QStringLiteral("Connect"));
    m_btnDisconnect = new QPushButton(QStringLiteral("Disconnect"));
    m_btnConnect->setObjectName(QStringLiteral("btnConnect"));
    m_btnDisconnect->setObjectName(QStringLiteral("btnDisconnect"));
    m_btnDisconnect->hide();

    m_btnLang = new QPushButton(QStringLiteral("EN"));
    m_btnLang->setObjectName(QStringLiteral("btnLang"));
    m_btnLang->setFixedSize(40, 30);
    m_btnLang->setToolTip(QStringLiteral("Switch to English"));

    connect(m_btnConnect,    &QPushButton::clicked,
            this, &MainWindow::onConnectClicked);
    connect(m_btnDisconnect, &QPushButton::clicked,
            this, &MainWindow::onDisconnectClicked);
    connect(m_btnLang,       &QPushButton::clicked,
            this, &MainWindow::onToggleLanguage);

    m_btnDfu = new QPushButton(QString::fromUtf8("固件升级"));
    m_btnDfu->setObjectName(QStringLiteral("btnDfu"));
    m_btnDfu->setToolTip(QString::fromUtf8("固件升级 (BOOTLOADER / DFU)"));
    connect(m_btnDfu, &QPushButton::clicked,
            this, &MainWindow::onFirmwareUpgradeClicked);

    m_btnWriteCardTab = new QPushButton(QString::fromUtf8("写入Aime卡"));
    m_btnWriteCardTab->setObjectName(QStringLiteral("btnDfu"));
    m_btnWriteCardTab->setCursor(Qt::PointingHandCursor);
    m_btnWriteCardTab->setEnabled(false);  // 连接后由 FW 版本决定
    m_btnWriteCardTab->setMinimumWidth(100);
    connect(m_btnWriteCardTab, &QPushButton::clicked, this, [this]() {
        if (!m_connected) return;
        m_stackedRight->setCurrentIndex(2);
        m_writeCardInput->setFocus();
    });

    hbox->addWidget(title);
    hbox->addWidget(btnTutorial);
    hbox->addSpacing(12);
    hbox->addStretch();
    hbox->addWidget(m_lblReaderModel);
    hbox->addSpacing(12);
    hbox->addWidget(m_lblConnDot);
    hbox->addWidget(m_lblConnStatus);
    hbox->addSpacing(20);
    hbox->addWidget(m_lblFwVersion);
    hbox->addStretch();
    hbox->addWidget(m_btnLang);
    hbox->addSpacing(8);
    hbox->addWidget(m_btnDfu);
    hbox->addSpacing(8);
    hbox->addWidget(m_btnWriteCardTab);
    hbox->addSpacing(8);
    hbox->addWidget(m_btnConnect);
    hbox->addWidget(m_btnDisconnect);

    return header;
}

// ====================================================================
//  左侧: 卡片监控
// ====================================================================

QWidget* MainWindow::buildCardMonitorPanel()
{
    QWidget *panel = new QWidget(this);
    panel->setObjectName(QStringLiteral("cardMonitorPanel"));
    QVBoxLayout *vbox = new QVBoxLayout(panel);
    vbox->setContentsMargins(16, 16, 8, 16);
    vbox->setSpacing(16);

    /* 卡片 UID 显示 */
    m_cardDisplayFrame = new QFrame(panel);
    m_cardDisplayFrame->setObjectName(QStringLiteral("cardDisplay"));
    m_cardDisplayFrame->setMinimumHeight(180);

    QVBoxLayout *displayLayout = new QVBoxLayout(m_cardDisplayFrame);
    displayLayout->setAlignment(Qt::AlignCenter);
    displayLayout->setSpacing(6);

    QLabel *uidTitle = new QLabel(QStringLiteral("CARD NUMBER"));
    uidTitle->setObjectName(QStringLiteral("cardUidTitle"));

    m_lblCardUid = new QLabel(QStringLiteral("Waiting for card..."));
    m_lblCardUid->setObjectName(QStringLiteral("cardUidValue"));
    m_lblCardUid->setAlignment(Qt::AlignCenter);
    m_lblCardUid->setWordWrap(false);

    m_lblCardNumber = new QLabel(this);
    m_lblCardNumber->setObjectName(QStringLiteral("cardNumberLabel"));
    m_lblCardNumber->setAlignment(Qt::AlignCenter);
    m_lblCardNumber->setWordWrap(false);
    m_lblCardNumber->hide();

    /* AIC 专用第二排: IDM 直接转十进制 */
    m_lblCardNumber2 = new QLabel(this);
    m_lblCardNumber2->setObjectName(QStringLiteral("cardNumberLabel2"));
    m_lblCardNumber2->setAlignment(Qt::AlignCenter);
    m_lblCardNumber2->setWordWrap(false);
    m_lblCardNumber2->hide();

    m_lblCardType = new QLabel(this);
    m_lblCardType->setObjectName(QStringLiteral("cardTypeLabel"));
    m_lblCardType->setAlignment(Qt::AlignCenter);
    m_lblCardType->hide();

    displayLayout->addWidget(uidTitle);
    displayLayout->addWidget(m_lblCardNumber);
    displayLayout->addWidget(m_lblCardNumber2);
    displayLayout->addWidget(m_lblCardUid);
    displayLayout->addWidget(m_lblCardType);

    vbox->addWidget(m_cardDisplayFrame);

    /* 原始数据日志 */
    QLabel *logTitle = new QLabel(QStringLiteral("RAW DATA LOG"));
    logTitle->setObjectName(QStringLiteral("sectionTitle"));

    m_rawLog = new QTextEdit(panel);
    m_rawLog->setObjectName(QStringLiteral("rawLog"));
    m_rawLog->setReadOnly(true);
    m_rawLog->setLineWrapMode(QTextEdit::NoWrap);
    m_rawLog->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    m_rawLog->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    m_rawLog->document()->setMaximumBlockCount(500);

    vbox->addWidget(logTitle);
    vbox->addWidget(m_rawLog, 1);

    return panel;
}

// ====================================================================
//  右侧: 设置面板
// ====================================================================

QWidget* MainWindow::buildSettingsPanel()
{
    QWidget *panel = new QWidget(this);
    panel->setObjectName(QStringLiteral("settingsPanel"));
    QVBoxLayout *vbox = new QVBoxLayout(panel);
    vbox->setContentsMargins(16, 16, 20, 16);
    vbox->setSpacing(16);

    QLabel *title = new QLabel(QStringLiteral("DEVICE SETTINGS"));
    title->setObjectName(QStringLiteral("sectionTitle"));

    m_tabSettings = new QTabWidget(panel);
    m_tabSettings->setObjectName(QStringLiteral("settingsTabs"));
    m_tabSettings->addTab(createSegaTab(),   QStringLiteral("SEGA"));
    m_tabSettings->addTab(createSpiceTab(),  QStringLiteral("SPICE"));
    m_tabSettings->addTab(createNamcoTab(),  QStringLiteral("NAMCO"));
    m_tabSettings->addTab(createSystemTab(), QStringLiteral("SYSTEM"));

    QHBoxLayout *btnRow = new QHBoxLayout();
    btnRow->setSpacing(10);

    m_btnRead  = new QPushButton(QStringLiteral("Read from Device"));
    m_btnWrite = new QPushButton(QStringLiteral("Write to Device"));
    m_btnReset = new QPushButton(QStringLiteral("Reset Defaults"));
    m_btnRead->setObjectName(QStringLiteral("btnPrimary"));
    m_btnWrite->setObjectName(QStringLiteral("btnPrimary"));
    m_btnReset->setObjectName(QStringLiteral("btnSecondary"));

    connect(m_btnRead,  &QPushButton::clicked,
            this, &MainWindow::onReadSettings);
    connect(m_btnWrite, &QPushButton::clicked,
            this, &MainWindow::onWriteSettings);
    connect(m_btnReset, &QPushButton::clicked,
            this, &MainWindow::onResetDefaults);

    btnRow->addWidget(m_btnRead);
    btnRow->addWidget(m_btnWrite);
    btnRow->addStretch();
    btnRow->addWidget(m_btnReset);

    vbox->addWidget(title);
    vbox->addWidget(m_tabSettings, 1);
    vbox->addLayout(btnRow);

    return panel;
}

/* ---- 固件升级页面 (BOOTLOADER / DFU 双通道) ---- */
QWidget* MainWindow::buildFirmwarePage()
{
    QWidget *w = new QWidget(this);
    w->setObjectName(QStringLiteral("dfuPage"));

    QVBoxLayout *vbox = new QVBoxLayout(w);
    vbox->setContentsMargins(20, 16, 20, 16);
    vbox->setSpacing(10);

    QLabel *title = new QLabel(QString::fromUtf8("固件升级"));
    title->setObjectName(QStringLiteral("sectionTitle"));

    QLabel *desc = new QLabel(QString::fromUtf8(
        "固件 v0.8 及以上走自研 BOOTLOADER（USB CDC / UART）；v0.7 及以下走 ST DFU。\n"
        "连接读卡器后打开本页会自动按版本选择通道；未连接时也可手动刷写救援。"));
    desc->setObjectName(QStringLiteral("tabDesc"));
    desc->setWordWrap(true);

    /* ================= BOOTLOADER 区 ================= */
    QGroupBox *bootBox = new QGroupBox(
        QString::fromUtf8("BOOTLOADER 升级  ·  固件 v0.8 及以上"), w);
    QVBoxLayout *bootLay = new QVBoxLayout(bootBox);
    bootLay->setSpacing(8);

    QHBoxLayout *bootEnterRow = new QHBoxLayout();
    bootEnterRow->setSpacing(8);

    m_bootEnterBtn = new QPushButton(QString::fromUtf8("进入 BOOTLOADER"), bootBox);
    m_bootEnterBtn->setObjectName(QStringLiteral("btnPrimary"));
    connect(m_bootEnterBtn, &QPushButton::clicked,
            this, &MainWindow::onEnterBootloader);

    m_bootPortCombo = new QComboBox(bootBox);
    m_bootPortCombo->setMinimumWidth(200);
    m_bootPortCombo->setToolTip(QString::fromUtf8(
        "BOOTLOADER 串口 (USB CDC 或 UART)。\n"
        "检测到 VID AFF1 / PID 52B2 的设备会自动选中。"));

    m_bootPortRefreshBtn = new QPushButton(QString::fromUtf8("刷新"), bootBox);
    m_bootPortRefreshBtn->setObjectName(QStringLiteral("btnSecondary"));
    m_bootPortRefreshBtn->setFixedWidth(56);
    connect(m_bootPortRefreshBtn, &QPushButton::clicked,
            this, &MainWindow::refreshPortCombo);

    bootEnterRow->addWidget(m_bootEnterBtn);
    bootEnterRow->addWidget(m_bootPortCombo, 1);
    bootEnterRow->addWidget(m_bootPortRefreshBtn);

    m_bootStateLabel = new QLabel(QString::fromUtf8("未检测到 BOOTLOADER"), bootBox);
    m_bootStateLabel->setObjectName(QStringLiteral("tabDesc"));

    QHBoxLayout *bootFileRow = new QHBoxLayout();
    bootFileRow->setSpacing(8);

    m_bootFileEdit = new QLineEdit(bootBox);
    m_bootFileEdit->setReadOnly(true);
    m_bootFileEdit->setPlaceholderText(QString::fromUtf8(
        "选择合并固件 (.bin) — 自动跳过前 24 KiB BOOTLOADER 头"));

    m_bootBrowseBtn = new QPushButton(QString::fromUtf8("浏览..."), bootBox);
    m_bootBrowseBtn->setObjectName(QStringLiteral("btnSecondary"));
    connect(m_bootBrowseBtn, &QPushButton::clicked,
            this, &MainWindow::onBootBrowse);

    bootFileRow->addWidget(m_bootFileEdit, 1);
    bootFileRow->addWidget(m_bootBrowseBtn);

    m_bootProgress = new QProgressBar(bootBox);
    m_bootProgress->setRange(0, 100);
    m_bootProgress->setValue(0);
    m_bootProgress->setFormat(QString::fromUtf8("%p%  %v/100"));
    m_bootProgress->hide();

    m_bootFlashBtn = new QPushButton(QString::fromUtf8("开始升级"), bootBox);
    m_bootFlashBtn->setObjectName(QStringLiteral("btnPrimary"));
    m_bootFlashBtn->setEnabled(false);
    connect(m_bootFlashBtn, &QPushButton::clicked,
            this, &MainWindow::onBootFlash);

    bootLay->addLayout(bootEnterRow);
    bootLay->addWidget(m_bootStateLabel);
    bootLay->addLayout(bootFileRow);
    bootLay->addWidget(m_bootProgress);
    bootLay->addWidget(m_bootFlashBtn);

    /* ================= DFU 区 ================= */
    QGroupBox *dfuBox = new QGroupBox(
        QString::fromUtf8("DFU 升级  ·  固件 v0.7 及以下"), w);
    QVBoxLayout *dfuLay = new QVBoxLayout(dfuBox);
    dfuLay->setSpacing(8);

    QHBoxLayout *dfuEnterRow = new QHBoxLayout();
    dfuEnterRow->setSpacing(8);

    m_dfuEnterBtn = new QPushButton(QString::fromUtf8("进入 DFU"), dfuBox);
    m_dfuEnterBtn->setObjectName(QStringLiteral("btnPrimary"));
    connect(m_dfuEnterBtn, &QPushButton::clicked,
            this, &MainWindow::onEnterDfu);

    dfuEnterRow->addWidget(m_dfuEnterBtn);
    dfuEnterRow->addStretch();

    m_dfuStateLabel = new QLabel(QString::fromUtf8("未检测到 DFU 设备"), dfuBox);
    m_dfuStateLabel->setObjectName(QStringLiteral("tabDesc"));

    QHBoxLayout *dfuFileRow = new QHBoxLayout();
    dfuFileRow->setSpacing(8);

    m_dfuFileEdit = new QLineEdit(dfuBox);
    m_dfuFileEdit->setReadOnly(true);
    m_dfuFileEdit->setPlaceholderText(
        QString::fromUtf8("选择固件文件 (.bin/.hex/.dfu) ..."));

    m_dfuBrowseBtn = new QPushButton(QString::fromUtf8("浏览..."), dfuBox);
    m_dfuBrowseBtn->setObjectName(QStringLiteral("btnSecondary"));
    connect(m_dfuBrowseBtn, &QPushButton::clicked,
            this, &MainWindow::onDfuFileBrowse);

    dfuFileRow->addWidget(m_dfuFileEdit, 1);
    dfuFileRow->addWidget(m_dfuBrowseBtn);

    m_dfuDownloadBtn = new QPushButton(
        QString::fromUtf8("未找到 dfu-util, 点击下载"), dfuBox);
    m_dfuDownloadBtn->setObjectName(QStringLiteral("btnSecondary"));
    m_dfuDownloadBtn->setVisible(false);
    connect(m_dfuDownloadBtn, &QPushButton::clicked,
            this, &MainWindow::onDfuCheckTool);

    m_dfuFlashBtn = new QPushButton(QString::fromUtf8("烧录固件"), dfuBox);
    m_dfuFlashBtn->setObjectName(QStringLiteral("btnPrimary"));
    m_dfuFlashBtn->setEnabled(false);
    connect(m_dfuFlashBtn, &QPushButton::clicked,
            this, &MainWindow::onDfuFlash);

    dfuLay->addLayout(dfuEnterRow);
    dfuLay->addWidget(m_dfuStateLabel);
    dfuLay->addLayout(dfuFileRow);
    dfuLay->addWidget(m_dfuDownloadBtn);
    dfuLay->addWidget(m_dfuFlashBtn);

    /* ================= 共用日志 + 重启 ================= */
    m_dfuLog = new QPlainTextEdit(w);
    m_dfuLog->setObjectName(QStringLiteral("dfuLog"));
    m_dfuLog->setReadOnly(true);
    m_dfuLog->setMaximumBlockCount(500);
    m_dfuLog->setStyleSheet(
        QStringLiteral("QPlainTextEdit { background:#1E1E2E; color:#CCCCCC; "
                       "font-family:Consolas,monospace; font-size:12px; }"));

    m_rebootBtn = new QPushButton(QString::fromUtf8("重启读卡器"), w);
    m_rebootBtn->setObjectName(QStringLiteral("btnSecondary"));
    m_rebootBtn->setEnabled(false);
    m_rebootBtn->setToolTip(QString::fromUtf8(
        "通过 BOOTLOADER 的 BOOT_APP 命令跳回应用固件"));
    connect(m_rebootBtn, &QPushButton::clicked,
            this, &MainWindow::onRebootReader);

    vbox->addWidget(title);
    vbox->addWidget(desc);
    vbox->addWidget(bootBox);
    vbox->addWidget(dfuBox);
    vbox->addWidget(m_dfuLog, 1);

    QHBoxLayout *bottomRow = new QHBoxLayout();
    bottomRow->addStretch();
    bottomRow->addWidget(m_rebootBtn);
    vbox->addLayout(bottomRow);

    /* ---- QProcess / 网络 ---- */
    m_dfuProcess = new QProcess(this);

    connect(m_netManager, &QNetworkAccessManager::finished,
            this, &MainWindow::onDfuDownloadFinished);

    /* ---- 串口列表每秒刷新 ---- */
    m_portTimer = new QTimer(this);
    m_portTimer->setInterval(1000);
    connect(m_portTimer, &QTimer::timeout,
            this, &MainWindow::refreshPortCombo);
    m_portTimer->start();

    /* ---- 手动切换串口后验证一次 ---- */
    connect(m_bootPortCombo,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) {
        if (m_suppressPortSignal || m_bootBusy)
            return;
        const QString port = m_bootPortCombo->currentData().toString();
        if (port.isEmpty())
            return;
        QMetaObject::invokeMethod(m_bootFlasher, "probe",
                                  Qt::QueuedConnection,
                                  Q_ARG(QString, port));
    });

    /* ---- 进入 BOOT/DFU 后的等待计时器 ---- */
    m_enterTimer = new QTimer(this);
    m_enterTimer->setInterval(250);
    connect(m_enterTimer, &QTimer::timeout,
            this, &MainWindow::pollEnterResult);

    return w;
}

/* ---- 写 Aime 卡页面 ---- */

QWidget* MainWindow::buildNormalTab()
{
    QWidget *tab = new QWidget(this);
    QVBoxLayout *vbox = new QVBoxLayout(tab);
    vbox->setContentsMargins(0, 12, 0, 0);
    vbox->setSpacing(10);

    QLabel *desc = new QLabel(QString::fromUtf8(
        "向空白 MIFARE 卡的 0 扇区写入 Aime 卡号和密钥。\n"
        "⏹ 只能写入空白卡，已有数据的卡无法覆盖\n"
        "⏹ 写入操作不可逆，请务必确认卡号无误\n"
        "⏹ 卡号必须为 20 位纯数字，不能以 3 开头"), tab);
    desc->setObjectName(QStringLiteral("tabDesc"));
    desc->setWordWrap(true);
    desc->setStyleSheet(QStringLiteral(
        "QLabel#tabDesc { color: #D32F2F; font-size: 13px; }"));

    QHBoxLayout *inputRow = new QHBoxLayout();
    QLabel *inputLabel = new QLabel(QString::fromUtf8("卡号 (20位):"), tab);
    inputLabel->setStyleSheet(QStringLiteral("font-weight: bold;"));
    m_writeCardInput = new QLineEdit(tab);
    m_writeCardInput->setPlaceholderText(
        QString::fromUtf8("请输入 20 位纯数字卡号"));
    m_writeCardInput->setMaxLength(20);
    m_writeCardInput->setStyleSheet(QStringLiteral(
        "QLineEdit { font-size: 16px; padding: 8px;"
        "border: 2px solid #ccc; border-radius: 4px; }"
        "QLineEdit:focus { border-color: #0078D4; }"));
    inputRow->addWidget(inputLabel);
    inputRow->addWidget(m_writeCardInput, 1);

    m_writeCardHint = new QLabel(tab);
    m_writeCardHint->setStyleSheet(QStringLiteral(
        "color: #D32F2F; font-size: 12px; min-height: 18px;"));

    connect(m_writeCardInput, &QLineEdit::textChanged, this,
            [this](const QString &text) {
        if (text.isEmpty()) {
            m_writeCardHint->clear();
            m_writeCardBtn->setEnabled(false);
            return;
        }
        if (text.length() != 20) {
            m_writeCardHint->setText(
                QString::fromUtf8("✘ 卡号长度不足 20 位"));
            m_writeCardBtn->setEnabled(false);
            return;
        }
        bool ok = true;
        for (const QChar &ch : text) {
            if (!ch.isDigit()) { ok = false; break; }
        }
        if (!ok) {
            m_writeCardHint->setText(
                QString::fromUtf8("✘ 卡号格式错误：必须为纯数字"));
            m_writeCardBtn->setEnabled(false);
            return;
        }
        if (text.startsWith(QLatin1Char('3'))) {
            m_writeCardHint->setText(
                QString::fromUtf8("✘ 卡号不能以 3 开头 (3 开头为 Banapass 卡号)"));
            m_writeCardBtn->setEnabled(false);
            return;
        }
        m_writeCardHint->setText(
            QString::fromUtf8("✓ 卡号格式正确，可以写入"));
        m_writeCardHint->setStyleSheet(QStringLiteral(
            "color: #2E7D32; font-size: 12px; min-height: 18px;"));
        m_writeCardBtn->setEnabled(true);
    });

    vbox->addWidget(desc);
    vbox->addLayout(inputRow);
    vbox->addWidget(m_writeCardHint);
    vbox->addStretch();
    return tab;
}

QWidget* MainWindow::buildAdvTab()
{
    QWidget *tab = new QWidget(this);
    QVBoxLayout *vbox = new QVBoxLayout(tab);
    vbox->setContentsMargins(0, 12, 0, 0);
    vbox->setSpacing(10);

    QLabel *desc = new QLabel(QString::fromUtf8(
        "高级模式：自定义扇区/区块/密钥/数据。\n"
        "⚠ 不管卡片状态如何，点击即发送写命令\n"
        "⚠ 不会等待空白卡，不检查卡是否在读卡器上\n"
        "⚠ 操作失误可能致卡片写死，请确定后操作"), tab);
    desc->setObjectName(QStringLiteral("tabDesc"));
    desc->setWordWrap(true);
    desc->setStyleSheet(QStringLiteral(
        "QLabel#tabDesc { color: #E65100; font-size: 13px; }"));

    QGridLayout *grid = new QGridLayout();
    grid->setSpacing(6);

    auto advStyle = QStringLiteral(
        "QLineEdit { font-family: Consolas,monospace; font-size: 14px;"
        "padding: 6px; border: 1px solid #aaa; border-radius: 3px; }"
        "QLineEdit:focus { border-color: #0078D4; }");

    grid->addWidget(new QLabel(QString::fromUtf8("扇区 (0-15):"), tab), 0, 0);
    m_advSector = new QLineEdit(QStringLiteral("0"), tab);
    m_advSector->setFixedWidth(60); m_advSector->setStyleSheet(advStyle);
    grid->addWidget(m_advSector, 0, 1);

    grid->addWidget(new QLabel(QString::fromUtf8("区块 (0-3):"), tab), 0, 2);
    m_advBlock = new QLineEdit(QStringLiteral("2"), tab);
    m_advBlock->setFixedWidth(60); m_advBlock->setStyleSheet(advStyle);
    grid->addWidget(m_advBlock, 0, 3);

    grid->addWidget(new QLabel(QString::fromUtf8("KeyA (12位HEX):"), tab), 1, 0);
    m_advKeyA = new QLineEdit(QStringLiteral("FFFFFFFFFFFF"), tab);
    m_advKeyA->setStyleSheet(advStyle);
    grid->addWidget(m_advKeyA, 1, 1, 1, 3);

    grid->addWidget(new QLabel(QString::fromUtf8("KeyB (12位HEX):"), tab), 2, 0);
    m_advKeyB = new QLineEdit(QStringLiteral("FFFFFFFFFFFF"), tab);
    m_advKeyB->setStyleSheet(advStyle);
    grid->addWidget(m_advKeyB, 2, 1, 1, 3);

    grid->addWidget(new QLabel(QString::fromUtf8("数据 (32位HEX):"), tab), 3, 0);
    m_advData = new QLineEdit(QStringLiteral("00000000000000000000000000000000"), tab);
    m_advData->setStyleSheet(advStyle);
    grid->addWidget(m_advData, 3, 1, 1, 3);

    m_advHint = new QLabel(tab);
    m_advHint->setStyleSheet(QStringLiteral("color: #D32F2F; font-size: 11px; min-height: 16px;"));
    grid->addWidget(m_advHint, 4, 0, 1, 4);

    // 高级写入按钮
    m_advWriteBtn = new QPushButton(QString::fromUtf8("直接写入"), tab);
    m_advWriteBtn->setObjectName(QStringLiteral("btnPrimary"));
    m_advWriteBtn->setStyleSheet(QStringLiteral(
        "QPushButton#btnPrimary { background-color: #E65100; color: white;"
        "font-weight: bold; padding: 10px 24px; border-radius: 4px; }"
        "QPushButton#btnPrimary:hover { background-color: #BF360C; }"));
    connect(m_advWriteBtn, &QPushButton::clicked,
            this, &MainWindow::doWriteAdv);

    vbox->addWidget(desc);
    vbox->addLayout(grid);
    vbox->addStretch();
    vbox->addWidget(m_advWriteBtn, 0, Qt::AlignRight);
    return tab;
}

QWidget* MainWindow::buildWriteCardPage()
{
    QWidget *w = new QWidget(this);
    QVBoxLayout *vbox = new QVBoxLayout(w);
    vbox->setContentsMargins(24, 24, 24, 24);
    vbox->setSpacing(10);

    QLabel *title = new QLabel(QString::fromUtf8("写入 Aime 卡号"), w);
    title->setObjectName(QStringLiteral("sectionTitle"));

    m_writeCardTabs = new QTabWidget(w);
    m_writeCardTabs->addTab(buildNormalTab(),
        QString::fromUtf8("普通写卡"));
    m_writeCardTabs->addTab(buildAdvTab(),
        QString::fromUtf8("高级写卡"));

    // 切换标签页时更新按钮状态
    connect(m_writeCardTabs, &QTabWidget::currentChanged, this,
            [this](int idx) {
        if (idx == 0) {
            m_writeCardBtn->setVisible(true);
            m_writeCardBtn->setEnabled(
                !m_writeCardInput->text().isEmpty()
                && m_writeCardInput->text().length() == 20
                && !m_writeCardInput->text().startsWith(QLatin1Char('3')));
        } else {
            m_writeCardBtn->setVisible(false);
        }
    });

    // 按钮行
    QHBoxLayout *btnRow = new QHBoxLayout();
    m_writeCardBackBtn = new QPushButton(
        QString::fromUtf8("返回设置"), w);
    m_writeCardBackBtn->setObjectName(QStringLiteral("btnSecondary"));
    connect(m_writeCardBackBtn, &QPushButton::clicked, this, [this]() {
        m_stackedRight->setCurrentIndex(0);
    });

    m_writeCardBtn = new QPushButton(
        QString::fromUtf8("写入卡片"), w);
    m_writeCardBtn->setObjectName(QStringLiteral("btnPrimary"));
    m_writeCardBtn->setEnabled(false);
    connect(m_writeCardBtn, &QPushButton::clicked,
            this, &MainWindow::onWriteCardClicked);

    btnRow->addWidget(m_writeCardBackBtn);
    btnRow->addStretch();
    btnRow->addWidget(m_writeCardBtn);

    vbox->addWidget(title);
    vbox->addWidget(m_writeCardTabs);
    vbox->addStretch();
    vbox->addLayout(btnRow);

    return w;
}

static QCheckBox* makeCheckBox(const QString &text, QWidget *parent)
{
    QCheckBox *cb = new QCheckBox(text, parent);
    cb->setObjectName(QStringLiteral("settingCheck"));
    return cb;
}

/* ---- SEGA ---- */
QWidget* MainWindow::createSegaTab()
{
    QWidget *w = new QWidget(this);
    QVBoxLayout *vbox = new QVBoxLayout(w);
    vbox->setContentsMargins(24, 24, 24, 24);
    vbox->setSpacing(14);

    QLabel *lbl = new QLabel(
        QStringLiteral("SEGA — Card type support settings for SEGA-compatible games."),
        w);
    lbl->setObjectName(QStringLiteral("tabDesc"));
    lbl->setWordWrap(true);
    vbox->addWidget(lbl);
    vbox->addSpacing(8);

    m_sega_tn32    = makeCheckBox(QString::fromUtf8(
        "启用 TN32 模拟。默认：模拟 15693 读卡器"), w);
    m_sega_aic     = makeCheckBox(QString::fromUtf8(
        "FeliCa AIC 卡号转换（转为真实序列号）"), w);
    m_sega_nesica  = makeCheckBox(QString::fromUtf8(
        "启用 NESiCA 支持（MIFARE Ultralight）"), w);
    m_sega_ea_pass = makeCheckBox(QString::fromUtf8(
        "启用 e-Amusement Pass 支持（ISO 15693）"), w);
    m_sega_tunion  = makeCheckBox(QString::fromUtf8(
        "启用交通联合支持（ISO 14443-A，APDU）"), w);
    m_sega_jubeat  = makeCheckBox(QString::fromUtf8(
        "启用乐动魔方支持（Jubeat China，MIFARE）"), w);

    vbox->addWidget(m_sega_tn32);
    vbox->addWidget(m_sega_aic);
    vbox->addWidget(m_sega_nesica);
    vbox->addWidget(m_sega_ea_pass);
    vbox->addWidget(m_sega_tunion);
    vbox->addWidget(m_sega_jubeat);
    vbox->addStretch();
    return w;
}

/* ---- SPICE ---- */
QWidget* MainWindow::createSpiceTab()
{
    QWidget *w = new QWidget(this);
    QVBoxLayout *vbox = new QVBoxLayout(w);
    vbox->setContentsMargins(24, 24, 24, 24);
    vbox->setSpacing(14);

    QLabel *lbl = new QLabel(
        QStringLiteral("SPICE — Card type support settings for SPICE-compatible games."),
        w);
    lbl->setObjectName(QStringLiteral("tabDesc"));
    lbl->setWordWrap(true);
    vbox->addWidget(lbl);
    vbox->addSpacing(8);

    m_spice_2p       = makeCheckBox(QString::fromUtf8("启用 2P 模式"), w);
    m_spice_aime     = makeCheckBox(QString::fromUtf8(
        "启用 Aime 支持（MIFARE Classic）"), w);
    m_spice_banapass = makeCheckBox(QString::fromUtf8(
        "启用 Banapass 支持（MIFARE Classic）"), w);
    m_spice_nesica   = makeCheckBox(QString::fromUtf8(
        "启用 NESiCA 支持（MIFARE Ultralight）"), w);
    m_spice_tunion   = makeCheckBox(QString::fromUtf8(
        "启用交通联合支持（ISO 14443-A，APDU）"), w);
    m_spice_jubeat   = makeCheckBox(QString::fromUtf8(
        "启用乐动魔方支持（Jubeat China，MIFARE）"), w);

    vbox->addWidget(m_spice_2p);
    vbox->addWidget(m_spice_aime);
    vbox->addWidget(m_spice_banapass);
    vbox->addWidget(m_spice_nesica);
    vbox->addWidget(m_spice_tunion);
    vbox->addWidget(m_spice_jubeat);
    vbox->addStretch();
    return w;
}

/* ---- NAMCO ---- */
QWidget* MainWindow::createNamcoTab()
{
    QWidget *w = new QWidget(this);
    QVBoxLayout *vbox = new QVBoxLayout(w);
    vbox->setContentsMargins(24, 24, 24, 24);
    vbox->setSpacing(14);

    QLabel *lbl = new QLabel(
        QStringLiteral("NAMCO — Card type support settings for NAMCO-compatible games."),
        w);
    lbl->setObjectName(QStringLiteral("tabDesc"));
    lbl->setWordWrap(true);
    vbox->addWidget(lbl);
    vbox->addSpacing(8);

    m_namco_todo     = makeCheckBox(QStringLiteral("（保留）"), w);
    m_namco_aime     = makeCheckBox(QString::fromUtf8(
        "启用 Aime 支持（MIFARE Classic）"), w);
    m_namco_banapass = makeCheckBox(QString::fromUtf8(
        "启用 Banapass 支持（MIFARE Classic）"), w);
    m_namco_nesica   = makeCheckBox(QString::fromUtf8(
        "启用 NESiCA 支持（MIFARE Ultralight）"), w);
    m_namco_tunion   = makeCheckBox(QString::fromUtf8(
        "启用交通联合支持（ISO 14443-B）"), w);
    m_namco_jubeat   = makeCheckBox(QString::fromUtf8(
        "启用乐动魔方支持（Jubeat China，MIFARE）"), w);

    vbox->addWidget(m_namco_todo);
    vbox->addWidget(m_namco_aime);
    vbox->addWidget(m_namco_banapass);
    vbox->addWidget(m_namco_nesica);
    vbox->addWidget(m_namco_tunion);
    vbox->addWidget(m_namco_jubeat);
    vbox->addStretch();
    return w;
}

/* ---- SYSTEM (去掉了 AIC 转换 — 已在 SEGA BIT1) ---- */
QWidget* MainWindow::createSystemTab()
{
    QWidget *w = new QWidget(this);
    QVBoxLayout *vbox = new QVBoxLayout(w);
    vbox->setContentsMargins(24, 24, 24, 24);
    vbox->setSpacing(18);

    QLabel *lbl = new QLabel(
        QStringLiteral("System — Hardware-level UART and LED settings."),
        w);
    lbl->setObjectName(QStringLiteral("tabDesc"));
    lbl->setWordWrap(true);
    vbox->addWidget(lbl);
    vbox->addSpacing(12);

    /* UART Baudrate */
    QGroupBox *grpUart = new QGroupBox(QString::fromUtf8("串口波特率"), w);
    grpUart->setObjectName(QStringLiteral("sysGroup"));
    QHBoxLayout *uartRow = new QHBoxLayout(grpUart);
    m_cmbUartBaud = new QComboBox(grpUart);
    m_cmbUartBaud->setObjectName(QStringLiteral("sysCombo"));
    m_cmbUartBaud->addItem(QStringLiteral("115200"), 0x00);
    m_cmbUartBaud->addItem(QStringLiteral("38400"),  0x01);
    m_cmbUartBaud->addItem(QStringLiteral("9600"),   0x02);
    m_cmbUartBaud->setMinimumWidth(200);
    uartRow->addWidget(m_cmbUartBaud);
    uartRow->addStretch();
    vbox->addWidget(grpUart);

    /* LED Brightness */
    QGroupBox *grpLed = new QGroupBox(QString::fromUtf8("LED 全局亮度"), w);
    grpLed->setObjectName(QStringLiteral("sysGroup"));
    QHBoxLayout *ledRow = new QHBoxLayout(grpLed);

    m_sldLedBright = new QSlider(Qt::Horizontal, grpLed);
    m_sldLedBright->setObjectName(QStringLiteral("ledSlider"));
    m_sldLedBright->setRange(0, 255);
    m_sldLedBright->setTickPosition(QSlider::TicksBelow);
    m_sldLedBright->setTickInterval(32);

    m_lblLedValue = new QLabel(QStringLiteral("0 / 255"), grpLed);
    m_lblLedValue->setObjectName(QStringLiteral("ledValue"));
    m_lblLedValue->setFixedWidth(70);
    m_lblLedValue->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    connect(m_sldLedBright, &QSlider::valueChanged, this, [this](int v) {
        m_lblLedValue->setText(QStringLiteral("%1 / 255").arg(v));
    });
    connect(m_sldLedBright, &QSlider::valueChanged,
            this, &MainWindow::onLedSliderChanged);

    QLabel *ledHint = new QLabel(
        QStringLiteral("0 = Off    |    128 = 50%    |    255 = Max"), grpLed);
    ledHint->setObjectName(QStringLiteral("sysHint"));

    ledRow->addWidget(m_sldLedBright, 1);
    ledRow->addSpacing(12);
    ledRow->addWidget(m_lblLedValue);
    vbox->addWidget(grpLed);
    vbox->addWidget(ledHint);
    vbox->addStretch();

    return w;
}

// ====================================================================
//  Bit ↔ Checkbox
// ====================================================================

void MainWindow::bindCheckboxesToBits(uint8_t byteVal,
                                      const QCheckBox *const *cbs, int count)
{
    for (int i = 0; i < count && i < 8; ++i) {
        if (cbs[i])
            const_cast<QCheckBox*>(cbs[i])->setChecked(byteVal & (1u << i));
    }
}

uint8_t MainWindow::collectBitsFromCheckboxes(
            const QCheckBox *const *cbs, int count) const
{
    uint8_t val = 0;
    for (int i = 0; i < count && i < 8; ++i) {
        if (cbs[i] && cbs[i]->isChecked())
            val |= (1u << i);
    }
    return val;
}

// ====================================================================
//  FlashData ↔ UI
// ====================================================================

void MainWindow::populateSettingsUI(const FlashData &data)
{
    const QCheckBox *sega[] = {
        m_sega_tn32, m_sega_aic, nullptr, m_sega_nesica,
        m_sega_ea_pass, m_sega_tunion, m_sega_jubeat
    };
    bindCheckboxesToBits(data.sega_setting, sega, 7);

    const QCheckBox *spice[] = {
        m_spice_2p, m_spice_aime, m_spice_banapass, m_spice_nesica,
        nullptr, m_spice_tunion, m_spice_jubeat
    };
    bindCheckboxesToBits(data.spice_setting, spice, 7);

    const QCheckBox *namco[] = {
        m_namco_todo, m_namco_aime, m_namco_banapass, m_namco_nesica,
        nullptr, m_namco_tunion, m_namco_jubeat
    };
    bindCheckboxesToBits(data.namco_setting, namco, 7);

    // UART (system_setting 即原来 system_setting_1)
    int baudIdx = m_cmbUartBaud->findData(data.system_setting);
    if (baudIdx >= 0)
        m_cmbUartBaud->setCurrentIndex(baudIdx);

    m_sldLedBright->blockSignals(true);
    m_sldLedBright->setValue(static_cast<int>(data.gobal_LED_setting));
    m_lblLedValue->setText(
        QStringLiteral("%1 / 255").arg(data.gobal_LED_setting));
    m_sldLedBright->blockSignals(false);
}

FlashData MainWindow::collectSettingsFromUI() const
{
    FlashData data;
    memset(&data, 0, sizeof(FlashData));

    const QCheckBox *sega[] = {
        m_sega_tn32, m_sega_aic, nullptr, m_sega_nesica,
        m_sega_ea_pass, m_sega_tunion, m_sega_jubeat
    };
    data.sega_setting = collectBitsFromCheckboxes(sega, 7);

    const QCheckBox *spice[] = {
        m_spice_2p, m_spice_aime, m_spice_banapass, m_spice_nesica,
        nullptr, m_spice_tunion, m_spice_jubeat
    };
    data.spice_setting = collectBitsFromCheckboxes(spice, 7);

    const QCheckBox *namco[] = {
        m_namco_todo, m_namco_aime, m_namco_banapass, m_namco_nesica,
        nullptr, m_namco_tunion, m_namco_jubeat
    };
    data.namco_setting = collectBitsFromCheckboxes(namco, 7);

    data.system_setting    = static_cast<uint8_t>(
        m_cmbUartBaud->currentData().toUInt());
    data.gobal_LED_setting = static_cast<uint8_t>(m_sldLedBright->value());

    return data;
}

void MainWindow::setControlsEnabled(bool enabled)
{
    m_tabSettings->setEnabled(enabled);
    m_btnRead->setEnabled(enabled);
    m_btnWrite->setEnabled(enabled);
    m_btnReset->setEnabled(enabled);
}

void MainWindow::updateConnectionUI(bool connected)
{
    if (connected) {
        m_lblConnDot->setProperty("status", "connected");
        m_lblConnStatus->setText(trC(QStringLiteral("已连接"),
                                     QStringLiteral("Connected")));
        m_btnConnect->hide();
        m_btnDisconnect->show();
    } else {
        m_lblConnDot->setProperty("status", "disconnected");
        m_lblConnStatus->setText(trC(QStringLiteral("未连接"),
                                     QStringLiteral("Disconnected")));
        m_btnConnect->show();
        m_btnDisconnect->hide();
    }
    m_lblConnDot->style()->unpolish(m_lblConnDot);
    m_lblConnDot->style()->polish(m_lblConnDot);
}

// ====================================================================
//  Flash 读取看门狗
//  连接成功后固件若在 FLASH_READ_TIMEOUT_MS 内不回 Flash_Read 响应,
//  判定为读卡器死机 (硬件原因) → 弹窗提示 + 复位为未连接状态。
// ====================================================================

void MainWindow::armFlashReadTimeout()
{
    if (!m_flashReadTimer) {
        m_flashReadTimer = new QTimer(this);
        m_flashReadTimer->setSingleShot(true);
        connect(m_flashReadTimer, &QTimer::timeout,
                this, &MainWindow::onFlashReadTimeout);
    }
    m_flashReadTimer->start(FLASH_READ_TIMEOUT_MS);
}

void MainWindow::disarmFlashReadTimeout()
{
    if (m_flashReadTimer)
        m_flashReadTimer->stop();
}

void MainWindow::onFlashReadTimeout()
{
    if (!m_connected) return;

    // 先复位连接 (stop() 内部会发出 deviceDisconnected → 状态复位为未连接)
    m_hid->stop();
    m_btnConnect->setEnabled(true);

    QMessageBox::warning(this,
        trC(QStringLiteral("连接超时"), QStringLiteral("Connection Timeout")),
        trC(QString::fromUtf8(
                "连接超时，读卡器可能已经死机（硬件原因）。\n\n"
                "请给读卡器断电后重新上电，再点击「连接」重试。"),
            QStringLiteral(
                "Connection timeout. The reader may have hung (hardware issue).\n\n"
                "Please power-cycle the reader and click Connect to retry.")));

    // 放在弹窗之后, 避免被 deviceDisconnected 的 "设备已断开" 覆盖
    m_lblStatusMsg->setText(trC(
        QStringLiteral("连接超时，读卡器可能已经死机（硬件原因）"),
        QStringLiteral("Timeout — the reader may have hung (hardware issue)")));
}

// ====================================================================
//  卡片 UID / 卡号 提取
// ====================================================================

QString MainWindow::cardTypeName(uint8_t cardType)
{
    switch (cardType) {
        case Card_None:          return QStringLiteral("(no card)");
        case Card_Aime:          return QStringLiteral("Aime (MIFARE)");
        case Card_Bana_New:      return QStringLiteral("Banapass (New)");
        case Card_Bana_Old:      return QStringLiteral("Banapass (Old)");
        case Card_AIC:           return QStringLiteral("AIC (FeliCa)");
        case Card_EPass:         return QStringLiteral("e-Pass (ISO15693)");
        case Card_Nesica:        return QStringLiteral("NESiCA (Ultralight)");
        case Card_Tunion:        return QStringLiteral("T-Union (14443-A)");
        case Card_Jubeat:        return QStringLiteral("Jubeat (MIFARE)");
        case Card_Unknow_Mifare: return QStringLiteral("Unknown MIFARE");
        case Card_Other_Felica:  return QStringLiteral("Other FeliCa");
        case Card_Blank_Mifare:  return QStringLiteral("Blank MIFARE");
        default:                 return QStringLiteral("Unknown (0x%1)")
                                        .arg(cardType, 2, 16, QLatin1Char('0'));
    }
}

QString MainWindow::extractUid(uint8_t cardType, const QByteArray &rawData)
{
    if (rawData.size() < 4) return QStringLiteral("(short)");

    switch (cardType) {
    case Card_Aime:
    case Card_Bana_New:
    case Card_Bana_Old:
    case Card_Jubeat:
    case Card_Tunion:
        // Card_RawData[0] 前 4 字节 = UID
        return QStringLiteral("%1 %2 %3 %4")
            .arg((uint8_t)rawData[0], 2, 16, QLatin1Char('0'))
            .arg((uint8_t)rawData[1], 2, 16, QLatin1Char('0'))
            .arg((uint8_t)rawData[2], 2, 16, QLatin1Char('0'))
            .arg((uint8_t)rawData[3], 2, 16, QLatin1Char('0'))
            .toUpper();

    case Card_AIC:
    case Card_EPass:
    case Card_Other_Felica:
        // Card_RawData[0] 前 8 字节 = UID/IDm
        if (rawData.size() < 8) break;
        {
            QStringList parts;
            for (int i = 0; i < 8; ++i)
                parts.append(QStringLiteral("%1")
                    .arg((uint8_t)rawData[i], 2, 16, QLatin1Char('0')));
            return parts.join(QLatin1Char(' ')).toUpper();
        }

    case Card_Nesica:
        // Card_RawData[0] 前 7 字节 = UID
        if (rawData.size() < 7) break;
        {
            QStringList parts;
            for (int i = 0; i < 7; ++i)
                parts.append(QStringLiteral("%1")
                    .arg((uint8_t)rawData[i], 2, 16, QLatin1Char('0')));
            return parts.join(QLatin1Char(' ')).toUpper();
        }
    }

    // 默认: 显示前 8 字节
    QStringList parts;
    int n = qMin(rawData.size(), 8);
    for (int i = 0; i < n; ++i)
        parts.append(QStringLiteral("%1")
            .arg((uint8_t)rawData[i], 2, 16, QLatin1Char('0')));
    return parts.join(QLatin1Char(' ')).toUpper();
}

/* 从 Card_RawData 提取明文卡号 (Card Number)
 *
 * Aime/Bana_New:  Card_RawData[2] 偏移6, 10字节HEX卡号
 * Bana_Old:       加密卡号 (不显示)
 * AIC:            Card_RawData[1] 加密 (不显示)
 * EPass:          Card_RawData[1] 偏移0, 10字节HEX卡号
 * Nesica:         Card_RawData[1] 偏移0, 10字节HEX卡号
 * Tunion:         Card_RawData[1] 偏移0, 10字节HEX卡号
 * Jubeat:         无明文卡号 (用UID构造)
 */
QString MainWindow::extractCardNumber(uint8_t cardType,
                                      const QByteArray &rawData)
{
    int offset = 0;
    int len    = 0;

    switch (cardType) {
    case Card_Aime:
    case Card_Bana_New:
        // Card_RawData[2] (offset 32) 偏移6, 10字节
        if (rawData.size() < 32 + 6 + 10) return QString();
        offset = 32 + 6;
        len    = 10;
        break;
    case Card_Bana_Old:
        // Card_RawData[1]/[2] 含加密数据, 用 Blowfish 解密
        // Block 1 = rawData[16..31], 16 字节 header
        if (rawData.size() >= 32) {
            int keynum = -1;
            const uint8_t *block1 = (const uint8_t*)rawData.constData() + 16;
            int32_t namcoId = banapassDecode(block1, &keynum);
            if (keynum >= 0) {
                return formatCardNumber(
                    banapassEncodeAccessCode(keynum, (uint32_t)namcoId));
            }
        }
        return QString();   // 解密失败
    case Card_AIC:
        // Card_RawData[1] (offset 16), 16 字节加密数据
        if (rawData.size() < 16 + 16) return QString();
        {
            uint8_t spad[16];
            memcpy(spad, rawData.constData() + 16, 16);
            // rawData[0..7] = FeliCa IDm (Card_RawData[0] 前半)
            decrypt_accesscode(spad,
                reinterpret_cast<const uint8_t*>(rawData.constData()));
            // 解密后 spad[6..15] 即为 20 位 BCD 卡号;
            // 若 spad[6] 不是合法 BCD 十位, decrypt_accesscode 已用 IDm 重新填充
            QString hex;
            for (int i = 6; i < 16; ++i)
                hex.append(QStringLiteral("%1")
                    .arg(spad[i], 2, 16, QLatin1Char('0')));
            return formatCardNumber(hex.toUpper());
        }
        break;
    case Card_Other_Felica:
        // UID 前 8 字节 → IDM 转卡号
        if (rawData.size() >= 8)
            return formatCardNumber(
                idmToAccessCode(
                    reinterpret_cast<const uint8_t*>(rawData.constData())));
        return QString();
    case Card_EPass:
    case Card_Nesica:
    case Card_Tunion:
        // Card_RawData[1] (offset 16) 偏移0, 10字节
        if (rawData.size() < 16 + 10) return QString();
        offset = 16;
        len    = 10;
        break;
    case Card_Jubeat:
        // 4 字节 UID → hex_to_accesscode (同 EPass 逻辑)
        if (rawData.size() >= 4) {
            uint8_t buf[8] = {};
            memcpy(buf, rawData.constData(), 4);   // 4 字节 UID, 后 4 字节补 0
            return uidToAccessCode(buf);
        }
        return QString();
    case Card_Unknow_Mifare:
    case Card_Blank_Mifare:
        return QString();
    default:
        return QString();
    }

    if (offset + len > rawData.size()) return QString();

    // 原始字节 → HEX 字符串, 每 4 字符加空格
    QString hex;
    for (int i = 0; i < len; ++i)
        hex.append(QStringLiteral("%1")
            .arg(static_cast<uint8_t>(rawData[offset + i]), 2, 16,
                 QLatin1Char('0')));
    return formatCardNumber(hex.toUpper());
}

/* IDM (8 字节大端) → 十进制字符串, 左补 0 到 20 位
 * 仅用于 AIC 卡的第二排显示, 不代表真实卡号。
 */
QString MainWindow::idmToDecimal(const QByteArray &idm)
{
    if (idm.size() < 8)
        return QString();

    quint64 v = 0;
    for (int i = 0; i < 8; ++i)
        v = (v << 8) | static_cast<quint8>(idm.at(i));

    return QString::number(v).rightJustified(20, QLatin1Char('0'));
}

void MainWindow::showCardInDisplay(const QString &uid, const QString &cardType,
                                   const QString &cardNumber,
                                   const QString &cardNumber2)
{
    // UID: 小字灰色
    m_lblCardUid->setText(uid);
    m_lblCardUid->setProperty("hasCard", true);
    m_lblCardUid->style()->unpolish(m_lblCardUid);
    m_lblCardUid->style()->polish(m_lblCardUid);

    // 卡号: 大字金色, 主要显示 (无卡号时显示占位符)
    if (!cardNumber.isEmpty()) {
        m_lblCardNumber->setText(cardNumber);
        m_lblCardNumber->setProperty("placeholder", false);
    } else {
        m_lblCardNumber->setText(
            QStringLiteral("---- ---- ---- ---- ----"));
        m_lblCardNumber->setProperty("placeholder", true);
    }
    m_lblCardNumber->style()->unpolish(m_lblCardNumber);
    m_lblCardNumber->style()->polish(m_lblCardNumber);
    m_lblCardNumber->show();

    // AIC 第二排: IDM 十六进制直接转十进制 (补零 20 位)
    if (!cardNumber2.isEmpty()) {
        m_lblCardNumber2->setText(cardNumber2);
        m_lblCardNumber2->show();
    } else {
        m_lblCardNumber2->clear();
        m_lblCardNumber2->hide();
    }

    m_lblCardType->setText(cardType);
    m_lblCardType->show();
}

void MainWindow::clearCardDisplay()
{
    m_lblCardUid->setText(trC(
        QStringLiteral("等待刷卡..."),
        QStringLiteral("Waiting for card...")));
    m_lblCardUid->setProperty("hasCard", false);
    m_lblCardUid->style()->unpolish(m_lblCardUid);
    m_lblCardUid->style()->polish(m_lblCardUid);

    m_lblCardNumber->hide();
    m_lblCardNumber2->clear();
    m_lblCardNumber2->hide();
    m_lblCardType->hide();
}

// ====================================================================
//  按钮点击
// ====================================================================

void MainWindow::onConnectClicked()
{
    m_stackedRight->setCurrentIndex(0);  // 切换回设置页
    m_btnDfu->show();
    m_lblStatusMsg->setText(trC(
        QStringLiteral("正在搜索设备..."),
        QStringLiteral("Searching for device...")));
    m_btnConnect->setEnabled(false);
    disarmFlashReadTimeout();
    m_hid->stop();
    m_hid->start();
}

void MainWindow::onDisconnectClicked()
{
    m_hid->stop();
}

void MainWindow::onReadSettings()
{
    m_lblStatusMsg->setText(trC(
        QStringLiteral("正在从设备读取设置..."),
        QStringLiteral("Reading settings from device...")));
    armFlashReadTimeout();          // 手动读取同样带超时
    m_hid->requestFlashRead();
}

void MainWindow::onWriteSettings()
{
    FlashData data = collectSettingsFromUI();
    data.firmversion_1 = m_flashData.firmversion_1;
    data.firmversion_2 = m_flashData.firmversion_2;

    auto result = QMessageBox::question(
        this,
        trC(QStringLiteral("确认写入"),
            QStringLiteral("Confirm Write")),
        trC(QString::fromUtf8(
            "即将向设备写入配置。\n\n此操作会更新读卡器内部 Flash 存储。\n确定继续？"),
            QStringLiteral(
            "About to write configuration to device.\n\n"
            "This will update the flash memory on the card reader.\n"
            "Are you sure?")));

    if (result != QMessageBox::Yes) return;

    m_lblStatusMsg->setText(trC(
        QStringLiteral("正在写入设置..."),
        QStringLiteral("Writing settings to device...")));
    m_hid->requestFlashWrite(data);
}

void MainWindow::onResetDefaults()
{
    auto result = QMessageBox::question(
        this,
        trC(QStringLiteral("恢复默认"),
            QStringLiteral("Reset Defaults")),
        trC(QStringLiteral("将所有设置恢复为出厂默认值？\n未保存的更改将丢失。"),
            QStringLiteral("Reset all settings to factory defaults?\n"
                           "Unsaved changes will be lost.")));

    if (result != QMessageBox::Yes) return;

    FlashData defaults;
    memset(&defaults, 0, sizeof(FlashData));
    defaults.firmversion_1      = 0x01;
    defaults.firmversion_2      = 0x00;
    defaults.system_setting     = 0x00;   // 115200
    defaults.gobal_LED_setting  = 0x80;   // 50%
    defaults.sega_setting       = 0x79;
    defaults.spice_setting      = 0x66;
    defaults.namco_setting      = 0x66;

    populateSettingsUI(defaults);
    m_lblStatusMsg->setText(trC(
        QStringLiteral("已恢复出厂默认值（尚未写入设备）"),
        QStringLiteral("Settings reset to defaults (not yet written)")));
}

void MainWindow::onLedSliderChanged(int value)
{
    if (!m_connected) return;
    uint8_t v = static_cast<uint8_t>(value);
    m_hid->requestLedColor(v, v, v);
}

/* ---- 原始数据日志 ---- */

void MainWindow::onRawDataSent(const QByteArray &data)
{
    appendLogLine(QStringLiteral("[Send]"),
                  data.toHex(' ').toUpper(),
                  QStringLiteral("#FFD54F"));
}

void MainWindow::onRawDataReceived(const QByteArray &data)
{
    appendLogLine(QStringLiteral("[Recv]"),
                  data.toHex(' ').toUpper(),
                  QStringLiteral("#00E676"));
}

void MainWindow::appendLogLine(const QString &prefix, const QString &hex,
                               const QString &color)
{
    if (!m_rawLog) return;
    m_rawLog->append(QStringLiteral(
        "<span style=\"color:%1;font-weight:600\">%2</span> "
        "<span style=\"color:#CCCCCC\">%3</span>")
        .arg(color, prefix, hex));
    // 自动滚到底部
    auto *sb = m_rawLog->verticalScrollBar();
    if (sb) sb->setValue(sb->maximum());
}

/* ====================================================================
 *  DFU 相关槽
 * ==================================================================== */

/* ====================================================================
 *  固件升级 — 页面入口与通道切换
 * ==================================================================== */

void MainWindow::onFirmwareUpgradeClicked()
{
    /* 再点一次回到设置页 (当作开关用, 否则进入后没有返回入口) */
    if (m_stackedRight->currentIndex() == 1) {
        m_stackedRight->setCurrentIndex(0);
        return;
    }

    m_stackedRight->setCurrentIndex(1);
    refreshPortCombo();

    /* 检测 dfu-util: 内置资源会自动释放到临时目录, 失败才弹下载框 */
    if (m_dfuToolPath.isEmpty())
        onDfuCheckTool();

    const bool bootHere = bootloaderPort(nullptr);
    const bool dfuHere  = dfuDevicePresent();

    updateFirmwareEntryState();

    /* 已经在某个升级通道里 → 不再重复动作 */
    if (bootHere || dfuHere)
        return;

    /* 没连接读卡器 → 什么都不干, 保留手动/救砖能力 */
    if (!m_connected) {
        m_lblStatusMsg->setText(trC(
            QString::fromUtf8("未连接读卡器 — 可手动选择串口，或将设备置于 DFU 后直接烧录"),
            QStringLiteral("Reader not connected — pick a port manually or flash in DFU")));
        return;
    }

    if (m_fwVersion == 0) {
        m_lblStatusMsg->setText(trC(
            QString::fromUtf8("尚未读取到固件版本，请稍候再试，或手动点「进入 BOOTLOADER」"),
            QStringLiteral("Firmware version not read yet — retry shortly")));
        return;
    }

    /* v0.8 及以上带自研 BOOTLOADER; v0.7 及以下走 ST DFU */
    if (m_fwVersion >= 0x08)
        onEnterBootloader();
    else
        onEnterDfu();
}

void MainWindow::onEnterBootloader()
{
    if (m_bootBusy)
        return;

    if (bootloaderPort(nullptr)) {
        QMessageBox::information(this,
            trC(QString::fromUtf8("已在 BOOTLOADER"),
                QStringLiteral("Already in Bootloader")),
            trC(QString::fromUtf8(
                    "读卡器已经处于 BOOTLOADER 模式，可以直接选择固件升级。"),
                QStringLiteral(
                    "The reader is already in the bootloader. Select a firmware file to upgrade.")));
        return;
    }

    if (!m_connected) {
        QMessageBox::warning(this,
            trC(QString::fromUtf8("未连接"),
                QStringLiteral("Not Connected")),
            trC(QString::fromUtf8(
                    "读卡器未连接，无法发送进入 BOOTLOADER 的命令。\n\n"
                    "如果设备已刷死，可手动选择串口后直接点「开始升级」。"),
                QStringLiteral(
                    "The reader is not connected, so the jump command cannot be sent.\n\n"
                    "If the device is bricked, pick a port manually and click Start.")));
        return;
    }

    /* v0.7 及以下没有 0x62 命令, 提前告知, 别让用户白等 10 秒 */
    if (m_fwVersion != 0 && m_fwVersion < 0x08) {
        auto r = QMessageBox::question(this,
            trC(QString::fromUtf8("固件不支持"),
                QStringLiteral("Firmware Not Supported")),
            trC(QString::fromUtf8(
                    "当前固件为 v%1.%2，低于 v0.8，不含自研 BOOTLOADER。\n\n"
                    "是否改用 DFU 方式升级？")
                .arg(m_fwVersion >> 4).arg(m_fwVersion & 0x0F),
                QStringLiteral(
                    "The current firmware is v%1.%2, older than v0.8 and has no bootloader.\n\n"
                    "Switch to DFU instead?")
                .arg(m_fwVersion >> 4).arg(m_fwVersion & 0x0F)),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
        if (r == QMessageBox::Yes) {
            onEnterDfu();
            return;
        }
    }

    /* 弹窗: 正在尝试进入 */
    m_enterMode       = 1;
    m_enterWaitedMs   = 0;
    m_enterNotified   = false;

    if (m_enterDlg) {
        m_enterDlg->close();
        m_enterDlg->deleteLater();
    }
    m_enterDlg = new QMessageBox(QMessageBox::Information,
        trC(QString::fromUtf8("固件升级"), QStringLiteral("Firmware Upgrade")),
        trC(QString::fromUtf8("正在尝试进入 BOOTLOADER…"),
            QStringLiteral("Trying to enter the bootloader…")),
        QMessageBox::NoButton, this);
    m_enterDlg->setWindowModality(Qt::WindowModal);
    m_enterDlg->show();

    m_enterTimer->start();

    m_lblStatusMsg->setText(trC(
        QString::fromUtf8("正在尝试进入 BOOTLOADER…"),
        QStringLiteral("Trying to enter the bootloader...")));

    m_hid->requestJumpToBoot();
}

void MainWindow::onEnterDfu()
{
    if (m_bootBusy)
        return;

    if (dfuDevicePresent()) {
        QMessageBox::information(this,
            trC(QString::fromUtf8("已在 DFU"),
                QStringLiteral("Already in DFU")),
            trC(QString::fromUtf8("已检测到处于 DFU 模式的设备。"),
                QStringLiteral("A device in DFU mode was detected.")));
        return;
    }

    if (!m_connected) {
        QMessageBox::warning(this,
            trC(QString::fromUtf8("未连接"),
                QStringLiteral("Not Connected")),
            trC(QString::fromUtf8(
                    "读卡器未连接，无法发送进入 DFU 的命令。\n\n"
                    "请手动让设备进入 DFU（例如按住 BOOT 键上电），"
                    "检测到 DFU 设备后即可直接烧录。"),
                QStringLiteral(
                    "The reader is not connected, so the DFU jump command cannot be sent.\n\n"
                    "Put the device into DFU manually; once detected you can flash it.")));
        return;
    }

    m_enterMode     = 2;
    m_enterWaitedMs = 0;
    m_enterNotified = false;

    if (m_enterDlg) {
        m_enterDlg->close();
        m_enterDlg->deleteLater();
    }
    m_enterDlg = new QMessageBox(QMessageBox::Information,
        trC(QString::fromUtf8("固件升级"), QStringLiteral("Firmware Upgrade")),
        trC(QString::fromUtf8("正在尝试进入 DFU 模式…"),
            QStringLiteral("Trying to enter DFU mode…")),
        QMessageBox::NoButton, this);
    m_enterDlg->setWindowModality(Qt::WindowModal);
    m_enterDlg->show();

    m_enterTimer->start();

    m_lblStatusMsg->setText(trC(
        QString::fromUtf8("正在尝试进入 DFU 模式…"),
        QStringLiteral("Trying to enter DFU mode...")));

    m_inDfuMode = true;
    m_hid->requestJumpToDfu();
}

/* 每 250 ms 检查一次是否已进入目标模式 */
void MainWindow::pollEnterResult()
{
    if (m_enterMode == 0) {
        m_enterTimer->stop();
        return;
    }

    m_enterWaitedMs += m_enterTimer->interval();

    const bool ok = (m_enterMode == 1) ? bootloaderPort(nullptr)
                                       : dfuDevicePresent();

    if (ok) {
        m_enterTimer->stop();
        refreshPortCombo();
        updateFirmwareEntryState();

        if (m_enterDlg) {
            m_enterDlg->setIcon(QMessageBox::Information);
            m_enterDlg->setText(m_enterMode == 1
                ? trC(QString::fromUtf8(
                          "已成功进入 BOOTLOADER。\n现在可以选择固件文件并开始升级。"),
                      QStringLiteral(
                          "Entered the bootloader successfully.\nSelect a firmware file to upgrade."))
                : trC(QString::fromUtf8(
                          "已成功进入 DFU 模式。\n现在可以选择固件文件并烧录。"),
                      QStringLiteral(
                          "Entered DFU mode successfully.\nSelect a firmware file to flash.")));
            m_enterDlg->setStandardButtons(QMessageBox::Ok);
        }

        m_lblStatusMsg->setText(m_enterMode == 1
            ? trC(QString::fromUtf8("已进入 BOOTLOADER，可以开始升级"),
                  QStringLiteral("In bootloader — ready to upgrade"))
            : trC(QString::fromUtf8("已进入 DFU，可以开始烧录"),
                  QStringLiteral("In DFU — ready to flash")));

        m_enterMode = 0;
        return;
    }

    /* 满 1 秒仍没看到 → 先告知一次, 然后继续等 (最多 10 秒) */
    if (m_enterWaitedMs >= 1000 && !m_enterNotified) {
        m_enterNotified = true;
        if (m_enterDlg) {
            m_enterDlg->setIcon(QMessageBox::Warning);
            m_enterDlg->setText(m_enterMode == 1
                ? trC(QString::fromUtf8(
                          "1 秒内尚未检测到 BOOTLOADER 串口。\n"
                          "设备可能仍在切换，继续等待中（最多 10 秒）…"),
                      QStringLiteral(
                          "No bootloader port yet after 1 s.\n"
                          "The device may still be switching — waiting up to 10 s..."))
                : trC(QString::fromUtf8(
                          "1 秒内尚未检测到 DFU 设备。\n"
                          "设备可能仍在重新枚举，继续等待中（最多 10 秒）…"),
                      QStringLiteral(
                          "No DFU device yet after 1 s.\n"
                          "The device may still be re-enumerating — waiting up to 10 s...")));
        }
    }

    if (m_enterWaitedMs >= 10000) {
        m_enterTimer->stop();
        if (m_enterDlg) {
            m_enterDlg->setIcon(QMessageBox::Critical);
            m_enterDlg->setText(m_enterMode == 1
                ? trC(QString::fromUtf8(
                          "进入 BOOTLOADER 失败。\n\n"
                          "可能原因：固件尚未包含自研 BOOTLOADER、USB 线不支持数据、"
                          "或设备已死机。\n可尝试拔插 USB 后重试。"),
                      QStringLiteral(
                          "Failed to enter the bootloader.\n\n"
                          "The firmware may lack the bootloader, the cable may be charge-only, "
                          "or the device may have hung.\nTry re-plugging the USB cable."))
                : trC(QString::fromUtf8(
                          "进入 DFU 失败。\n\n"
                          "请尝试手动进入 DFU：断电后按住 BOOT 键再上电。"),
                      QStringLiteral(
                          "Failed to enter DFU.\n\n"
                          "Try entering DFU manually: hold BOOT while powering on.")));
            m_enterDlg->setStandardButtons(QMessageBox::Ok);
        }

        m_lblStatusMsg->setText(trC(
            QString::fromUtf8("进入升级模式失败"),
            QStringLiteral("Failed to enter upgrade mode")));

        m_enterMode = 0;
    }
}

/* ---- 串口下拉框实时刷新 ---- */

void MainWindow::refreshPortCombo()
{
    if (!m_bootPortCombo)
        return;

    const auto ports = PortScan::listComPorts();

    /* 列表没变化就不重建, 避免打断用户展开的下拉框 */
    QString signature;
    for (const ComPortInfo &p : ports)
        signature += p.portName + QLatin1Char('|') + p.description
                   + QLatin1Char('|') + QString::number(p.vid) + QLatin1Char(':')
                   + QString::number(p.pid) + QLatin1Char(';');

    const bool bootHere = bootloaderPort(nullptr);
    signature += bootHere ? QStringLiteral("#boot") : QStringLiteral("#noboot");
    signature += dfuDevicePresent() ? QStringLiteral("#dfu") : QStringLiteral("#nodfu");

    if (signature == m_portSignature) {
        updateFirmwareEntryState();
        return;
    }
    m_portSignature = signature;

    const QString prev = m_bootPortCombo->currentData().toString();

    m_suppressPortSignal = true;
    m_bootPortCombo->clear();

    int autoIdx = -1;
    for (const ComPortInfo &p : ports) {
        m_bootPortCombo->addItem(p.displayName(), p.portName);
        if (p.isBootloader() && autoIdx < 0)
            autoIdx = m_bootPortCombo->count() - 1;
    }

    if (ports.isEmpty()) {
        m_bootPortCombo->addItem(
            trC(QString::fromUtf8("(无可用串口)"),
                QStringLiteral("(no serial ports)")),
            QString());
    } else if (autoIdx >= 0) {
        m_bootPortCombo->setCurrentIndex(autoIdx);
    } else if (!prev.isEmpty()) {
        const int idx = m_bootPortCombo->findData(prev);
        if (idx >= 0)
            m_bootPortCombo->setCurrentIndex(idx);
    }

    m_suppressPortSignal = false;

    updateFirmwareEntryState();
}

void MainWindow::updateFirmwareEntryState()
{
    if (!m_bootEnterBtn || !m_dfuEnterBtn)
        return;

    ComPortInfo boot;
    const bool bootHere = bootloaderPort(&boot);
    const bool dfuHere  = dfuDevicePresent();
    const bool anyMode  = bootHere || dfuHere;

    /* 任一通道已连接 → 两个「进入」按钮都置灰 */
    m_bootEnterBtn->setEnabled(!anyMode && !m_bootBusy);
    m_dfuEnterBtn->setEnabled(!anyMode && !m_bootBusy);

    if (bootHere) {
        m_bootStateLabel->setText(trC(
            QString::fromUtf8("● 已连接 BOOTLOADER — %1").arg(boot.displayName()),
            QStringLiteral("● Bootloader connected — %1").arg(boot.displayName())));
    } else {
        m_bootStateLabel->setText(trC(
            QString::fromUtf8("○ 未检测到 BOOTLOADER"),
            QStringLiteral("○ No bootloader detected")));
    }

    m_dfuStateLabel->setText(dfuHere
        ? trC(QString::fromUtf8("● 已检测到 DFU 设备"),
              QStringLiteral("● DFU device detected"))
        : trC(QString::fromUtf8("○ 未检测到 DFU 设备"),
              QStringLiteral("○ No DFU device detected")));

    m_bootFlashBtn->setEnabled(!m_bootBusy && !m_bootFilePath.isEmpty());
    m_rebootBtn->setEnabled(bootHere && !m_bootBusy);
}

bool MainWindow::bootloaderPort(ComPortInfo *out) const
{
    return PortScan::findBootloaderPort(out);
}

bool MainWindow::dfuDevicePresent() const
{
    /* 读卡器的 DFU 描述符: VID AFF1 / PID DF11;
     * 也兼容 ST 官方驱动的 0483:DF11 */
    return PortScan::isUsbPresent(BOOT_USB_VID, BOOT_USB_PID_DFU)
        || PortScan::isUsbPresent(0x0483, 0xDF11);
}

/* ---- BOOTLOADER 烧录 ---- */

void MainWindow::onBootBrowse()
{
    QString path = QFileDialog::getOpenFileName(this,
        trC(QString::fromUtf8("选择固件文件"),
            QStringLiteral("Select Firmware File")),
        QString(),
        QStringLiteral("Firmware (*.bin);;All Files (*)"));
    if (path.isEmpty())
        return;

    /* BOOT 通道只接受「BOOTLOADER + APP」合并固件。
     * 纯 APP 镜像(0x6000 处不是向量表)一律拒绝, 让它去 DFU 通道。
     */
    const BootImageKind kind = classifyFirmwareImage(path);
    if (kind != BootImageKind::Merged) {
        QMessageBox::warning(this,
            trC(QString::fromUtf8("固件不能走 BOOT"),
                QStringLiteral("Not a Bootloader Image")),
            kind == BootImageKind::AppOnly
                ? trC(QString::fromUtf8(
                          "该文件是纯 APP 镜像（偏移 0x6000 处不是合法的 APP 向量表），"
                          "不带 BOOTLOADER 头。\n\n"
                          "BOOT 通道只能烧录「BOOTLOADER + APP」的合并固件。\n"
                          "请改用下方 DFU 通道烧录该文件。"),
                      QStringLiteral(
                          "This is a plain application image with no bootloader header.\n\n"
                          "The BOOT channel only accepts merged bootloader+app images.\n"
                          "Use the DFU channel below to flash this file."))
                : trC(QString::fromUtf8(
                          "该文件不是有效的固件镜像（向量表不合法），已拒绝。"),
                      QStringLiteral(
                          "This file is not a valid firmware image (bad vector table).")));
        return;
    }

    m_bootFilePath = path;
    m_bootFileEdit->setText(path);
    updateFirmwareEntryState();
}

void MainWindow::onBootFlash()
{
    if (m_bootBusy || m_bootFilePath.isEmpty())
        return;

    const QString port = m_bootPortCombo->currentData().toString();
    if (port.isEmpty()) {
        QMessageBox::warning(this,
            trC(QString::fromUtf8("未选择串口"), QStringLiteral("No Port")),
            trC(QString::fromUtf8("请先在下拉框中选择 BOOTLOADER 的串口。"),
                QStringLiteral("Select the bootloader serial port first.")));
        return;
    }

    if (!bootloaderPort(nullptr)) {
        auto r = QMessageBox::question(this,
            trC(QString::fromUtf8("确认烧录"), QStringLiteral("Confirm Flash")),
            trC(QString::fromUtf8(
                    "当前没有检测到 BOOTLOADER 设备。\n\n"
                    "仍要在 %1 上尝试烧录吗？").arg(port),
                QStringLiteral(
                    "No bootloader detected.\n\n"
                    "Try flashing on %1 anyway?").arg(port)),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (r != QMessageBox::Yes)
            return;
    }

    /* 硬性检查: 没有 BOOTLOADER 头的固件不能走 BOOT 通道 */
    const BootImageKind kind = classifyFirmwareImage(m_bootFilePath);
    if (kind != BootImageKind::Merged) {
        QMessageBox::critical(this,
            trC(QString::fromUtf8("固件不能走 BOOT"),
                QStringLiteral("Not a Bootloader Image")),
            kind == BootImageKind::AppOnly
                ? trC(QString::fromUtf8(
                          "该固件不带 BOOTLOADER 头，不能通过 BOOT 通道烧录。\n\n"
                          "请改用 DFU 通道烧录（DFU 可以刷任何版本）。"),
                      QStringLiteral(
                          "This firmware has no bootloader header and cannot be flashed "
                          "over BOOT.\n\nUse the DFU channel instead — DFU accepts any version."))
                : trC(QString::fromUtf8(
                          "该文件不是有效的固件镜像，已中止。"),
                      QStringLiteral("Not a valid firmware image — aborted.")));
        return;
    }

    m_bootBusy = true;
    m_bootProgress->setValue(0);
    m_bootProgress->show();
    m_dfuLog->clear();
    updateFirmwareEntryState();

    QMetaObject::invokeMethod(m_bootFlasher, "flash", Qt::QueuedConnection,
                              Q_ARG(QString, port),
                              Q_ARG(QString, m_bootFilePath),
                              Q_ARG(bool, true));
}

void MainWindow::onBootLog(const QString &line)
{
    if (m_dfuLog)
        m_dfuLog->appendPlainText(line);
}

void MainWindow::onBootProgress(int percent, const QString &detail)
{
    if (!m_bootProgress)
        return;
    m_bootProgress->setValue(qBound(0, percent, 100));
    m_bootProgress->setFormat(QStringLiteral("%1%  %2").arg(percent).arg(detail));
}

void MainWindow::onBootFlashFinished(bool ok, const QString &error)
{
    m_bootBusy = false;

    if (ok) {
        m_bootProgress->setValue(100);
        m_bootProgress->setFormat(QString::fromUtf8("100%  烧录完成"));
        refreshPortCombo();

        QMessageBox::information(this,
            trC(QString::fromUtf8("升级完成"), QStringLiteral("Upgrade Complete")),
            trC(QString::fromUtf8(
                    "固件烧录完成并已校验通过。\n\n"
                    "设备仍停留在 BOOTLOADER，请点「重启读卡器」启动新固件。"),
                QStringLiteral(
                    "Firmware flashed and verified.\n\n"
                    "The device is still in the bootloader — click Restart Reader to boot the new firmware.")));
    } else {
        m_bootProgress->hide();
        updateFirmwareEntryState();

        QMessageBox::critical(this,
            trC(QString::fromUtf8("升级失败"), QStringLiteral("Upgrade Failed")),
            trC(QString::fromUtf8("固件升级失败：\n\n%1").arg(error),
                QStringLiteral("Firmware upgrade failed:\n\n%1").arg(error)));
        return;
    }

    updateFirmwareEntryState();
}

/* ---- 重启读卡器 ---- */

void MainWindow::onRebootReader()
{
    if (m_bootBusy)
        return;

    const QString port = m_bootPortCombo->currentData().toString();
    if (port.isEmpty()) {
        QMessageBox::warning(this,
            trC(QString::fromUtf8("未选择串口"), QStringLiteral("No Port")),
            trC(QString::fromUtf8("请先选择 BOOTLOADER 的串口。"),
                QStringLiteral("Select the bootloader serial port first.")));
        return;
    }

    m_bootBusy = true;
    updateFirmwareEntryState();
    m_dfuLog->appendPlainText(trC(
        QString::fromUtf8("正在发送 BOOT_APP，重启读卡器…"),
        QStringLiteral("Sending BOOT_APP, restarting the reader...")));

    QMetaObject::invokeMethod(m_bootFlasher, "bootApp", Qt::QueuedConnection,
                              Q_ARG(QString, port));
}

void MainWindow::onRebootFinished(bool ok, const QString &error)
{
    m_bootBusy = false;
    refreshPortCombo();
    updateFirmwareEntryState();

    if (ok) {
        m_dfuLog->appendPlainText(trC(
            QString::fromUtf8("已重启，设备应重新枚举为读卡器。"),
            QStringLiteral("Restarted — the device should re-enumerate as a reader.")));
        m_lblStatusMsg->setText(trC(
            QString::fromUtf8("读卡器已重启，请点「连接」重新连接"),
            QStringLiteral("Reader restarted — click Connect to reconnect")));

        QMessageBox::information(this,
            trC(QString::fromUtf8("重启完成"), QStringLiteral("Restart Sent")),
            trC(QString::fromUtf8(
                    "已发送重启命令。\n\n"
                    "设备会重新枚举为读卡器，请点右上角「连接」重新连接。"),
                QStringLiteral(
                    "Restart command sent.\n\n"
                    "The device will re-enumerate as a reader. Click Connect to reconnect.")));
    } else {
        QMessageBox::critical(this,
            trC(QString::fromUtf8("重启失败"), QStringLiteral("Restart Failed")),
            trC(QString::fromUtf8("无法重启读卡器：\n\n%1").arg(error),
                QStringLiteral("Failed to restart the reader:\n\n%1").arg(error)));
    }
}

/* ---- 手动选择串口后的探测结果 ---- */

void MainWindow::onProbeFinished(bool ok, const QString &summary)
{
    if (!m_dfuLog)
        return;

    m_dfuLog->appendPlainText(ok
        ? trC(QString::fromUtf8("[探测] 串口确认是 BOOTLOADER — %1").arg(summary),
              QStringLiteral("[probe] bootloader confirmed — %1").arg(summary))
        : trC(QString::fromUtf8("[探测] 该串口不是 BOOTLOADER — %1").arg(summary),
              QStringLiteral("[probe] not a bootloader — %1").arg(summary)));
}

void MainWindow::onDfuFileBrowse()
{
    QString path = QFileDialog::getOpenFileName(this,
        trC(QString::fromUtf8("选择固件文件"),
            QStringLiteral("Select Firmware File")),
        QString(),
        QStringLiteral("Firmware Files (*.bin *.hex *.dfu);;All Files (*)"));
    if (!path.isEmpty()) {
        m_dfuFilePath = path;
        m_dfuFileEdit->setText(path);
        m_dfuFlashBtn->setEnabled(true);
    }
}

void MainWindow::onDfuFlash()
{
    if (m_dfuFilePath.isEmpty()) return;

    if (m_dfuToolPath.isEmpty()) {
        onDfuCheckTool();
        if (m_dfuToolPath.isEmpty())
            return;
    }

    m_dfuFlashBtn->setEnabled(false);
    m_dfuLog->clear();

    // Step 1: 列出 DFU 设备 (异步)
    m_dfuLog->appendPlainText(QString::fromUtf8("正在扫描 DFU 设备...\n"));

    QString dfuExe = m_dfuToolPath.isEmpty()
        ? QStringLiteral("dfu-util") : m_dfuToolPath;

    connect(m_dfuProcess,
            QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &MainWindow::onDfuListFinished,
            Qt::SingleShotConnection);

    m_dfuProcess->start(dfuExe, {QStringLiteral("-l")});
}

void MainWindow::onDfuListFinished(int, QProcess::ExitStatus)
{
    QString out = QString::fromUtf8(m_dfuProcess->readAllStandardOutput());
    m_dfuLog->appendPlainText(out);

    if (!out.contains(QStringLiteral("Found DFU"))) {
        m_dfuLog->appendPlainText(
            QString::fromUtf8("[错误] 未检测到 DFU 设备\n"
            "请确认设备已进入 DFU 模式且 USB 驱动已安装\n"));
        m_dfuFlashBtn->setEnabled(true);
        return;
    }

    // Step 2: 烧录
    QString dfuExe = m_dfuToolPath.isEmpty()
        ? QStringLiteral("dfu-util") : m_dfuToolPath;
    QStringList args;
    args << QStringLiteral("-a") << QStringLiteral("0")
         << QStringLiteral("-s") << QStringLiteral("0x08000000:leave")
         << QStringLiteral("-D") << m_dfuFilePath;

    m_dfuLog->appendPlainText(
        QStringLiteral("\n$ dfu-util %1\n").arg(args.join(QLatin1Char(' '))));

    connect(m_dfuProcess,
            QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &MainWindow::onDfuFlashFinished,
            Qt::SingleShotConnection);

    m_dfuProcess->start(dfuExe, args);
}

void MainWindow::onDfuFlashFinished(int exitCode, QProcess::ExitStatus)
{
    QString out = QString::fromUtf8(m_dfuProcess->readAllStandardOutput());
    QString err = QString::fromUtf8(m_dfuProcess->readAllStandardError());
    m_dfuLog->appendPlainText(out);
    if (!err.isEmpty()) m_dfuLog->appendPlainText(err);

    if (exitCode != 0) {
        m_dfuLog->appendPlainText(
            QString::fromUtf8("\n[失败] dfu-util 返回错误码 %1\n"
            "提示: 如刷了错误固件导致变砖, 请用 STM32CubeProgrammer 救砖\n")
                .arg(exitCode));
        m_dfuFlashBtn->setEnabled(true);
        return;
    }

    // Step 3: 复位
    m_dfuLog->appendPlainText(
        QString::fromUtf8("\n烧录完成, 正在复位设备...\n"));

    QString dfuExe = m_dfuToolPath.isEmpty()
        ? QStringLiteral("dfu-util") : m_dfuToolPath;

    connect(m_dfuProcess,
            QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &MainWindow::onDfuResetFinished,
            Qt::SingleShotConnection);

    QTimer::singleShot(500, this, [this, dfuExe]() {
        m_dfuProcess->start(dfuExe, {QStringLiteral("-e")});
    });
}

void MainWindow::onDfuResetFinished(int, QProcess::ExitStatus)
{
    m_dfuLog->appendPlainText(
        QString::fromUtf8("[成功] 固件烧录完成！\n"
        "如设备无法识别请重新拔插 USB。"));

    QMessageBox::information(this,
        trC(QString::fromUtf8("完成"), QStringLiteral("Done")),
        trC(QString::fromUtf8(
            "固件烧录完成。\n\n"
            "请重新拔插设备 USB 线后再点击「连接」。"),
            QStringLiteral(
            "Firmware flashed.\n\n"
            "Please reconnect the USB cable, then click Connect.")));

    m_dfuFlashBtn->setEnabled(true);
}

/* ---- dfu-util 自动下载 ---- */

static const QStringList g_dfuMirrors = {
    QStringLiteral("https://gitee.com/Reimages/dfu-util/releases/download/v0.1/dfu-util.exe"),
};
static int g_dfuMirrorIdx = 0;

void MainWindow::onDfuCheckTool()
{
    // 1. 查找 dfu-util.exe: 优先看 exe 同目录和 tools/
    QStringList searchPaths;
    searchPaths << QCoreApplication::applicationDirPath() + QStringLiteral("/dfu-util.exe")
                << QCoreApplication::applicationDirPath() + QStringLiteral("/tools/dfu-util.exe");

    for (const auto &p : searchPaths) {
        if (QFile::exists(p)) {
            m_dfuToolPath = p;
            m_dfuDownloadBtn->setVisible(false);
            m_dfuLog->appendPlainText(
                QString::fromUtf8("[OK] 找到 dfu-util: %1\n").arg(p));
            return;
        }
    }

    // 2. 从内置资源释放 dfu-util 及依赖 DLL 到临时目录
    QString tempPath = QStandardPaths::writableLocation(
        QStandardPaths::TempLocation)
        + QStringLiteral("/monica_nfc/dfu-util.exe");
    QDir().mkpath(QFileInfo(tempPath).absolutePath());

    // 需要释放的文件列表 (资源别名 → 文件名)
    struct { const char *res; const char *name; } dfuFiles[] = {
        { ":/dfu/dfu-util.exe",         "dfu-util.exe" },
        { ":/dfu/libusb-1.0.dll",       "libusb-1.0.dll" },
        { ":/dfu/libwinpthread-1.dll",  "libwinpthread-1.dll" },
    };
    QString dfuDir = QFileInfo(tempPath).absolutePath();
    bool allOk = true;
    for (auto &f : dfuFiles) {
        QFile resFile(QString::fromLatin1(f.res));
        QFile outFile(dfuDir + QStringLiteral("/") + QString::fromLatin1(f.name));
        if (resFile.open(QIODevice::ReadOnly)
            && outFile.open(QIODevice::WriteOnly)) {
            outFile.write(resFile.readAll());
            outFile.close();
        } else {
            allOk = false;
        }
    }

    if (allOk) {
        m_dfuToolPath = tempPath;
        m_dfuDownloadBtn->setVisible(false);
        m_dfuLog->appendPlainText(
            QString::fromUtf8("[OK] dfu-util 已从内置资源释放到 %1\n").arg(dfuDir));
        return;
    }

    // 3. 都没找到 → 显示下载按钮（兜底）
    m_dfuDownloadBtn->setVisible(true);
    m_dfuDownloadBtn->setText(
        QString::fromUtf8("未找到 dfu-util.exe，点击自动下载"));
    m_dfuDownloadBtn->setToolTip(QString::fromUtf8(
        "将从可信任源下载 dfu-util 到程序目录"));

    g_dfuMirrorIdx = 0;
    auto result = QMessageBox::question(this,
        trC(QString::fromUtf8("缺少工具"),
            QStringLiteral("Missing Tool")),
        trC(QString::fromUtf8(
            "未找到 dfu-util.exe，无法进行固件烧录。\n\n"
            "是否自动从可信任源下载？"),
            QStringLiteral(
            "dfu-util.exe not found.\n\n"
            "Download automatically from a trusted source?")),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);

    if (result == QMessageBox::Yes) {
        tryDownloadDfuUtil();
    }
}

void MainWindow::tryDownloadDfuUtil()
{
    if (m_dlProgress) {
        m_dlProgress->close();
        delete m_dlProgress;
    }

    m_dlProgress = new QProgressDialog(
        trC(QString::fromUtf8("正在下载 dfu-util..."),
            QStringLiteral("Downloading dfu-util...")),
        trC(QStringLiteral("取消"), QStringLiteral("Cancel")),
        0, 0, this);
    m_dlProgress->setWindowModality(Qt::WindowModal);
    m_dlProgress->show();

    m_dfuLog->appendPlainText(
        QString::fromUtf8("正在下载 dfu-util...\n"));

    QUrl url(g_dfuMirrors.at(g_dfuMirrorIdx));
    QNetworkRequest req(url);
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);
    m_netManager->get(req);
}

void MainWindow::onDfuDownloadFinished(QNetworkReply *reply)
{
    if (m_dlProgress) {
        m_dlProgress->close();
        delete m_dlProgress;
        m_dlProgress = nullptr;
    }

    QByteArray data = reply->readAll();
    int status = reply->attribute(
        QNetworkRequest::HttpStatusCodeAttribute).toInt();

    if (reply->error() != QNetworkReply::NoError || status >= 400) {
        m_dfuLog->appendPlainText(
            QString::fromUtf8("[下载失败] %1 (HTTP %2)\n")
                .arg(reply->errorString()).arg(status));

        // 尝试下一个镜像
        g_dfuMirrorIdx++;
        if (g_dfuMirrorIdx < g_dfuMirrors.size()) {
            m_dfuLog->appendPlainText(
                QString::fromUtf8("尝试备用源: %1\n")
                    .arg(g_dfuMirrors.at(g_dfuMirrorIdx)));
            tryDownloadDfuUtil();
        } else {
            m_dfuLog->appendPlainText(
                QString::fromUtf8(
                    "[错误] 所有下载源均已失败。\n"
                    "请手动下载 dfu-util.exe 并放到程序目录下。\n"));
        }
        reply->deleteLater();
        return;
    }

    // 验证不是 HTML 错误页
    if (data.size() < 1024) {
        m_dfuLog->appendPlainText(
            QString::fromUtf8("[错误] 下载的文件过小 (%1 字节)，可能不是有效的 exe。\n")
                .arg(data.size()));
        reply->deleteLater();
        return;
    }

    // 保存到 exe 同目录
    QString savePath = QCoreApplication::applicationDirPath()
                       + QStringLiteral("/dfu-util.exe");
    QFile file(savePath);
    if (!file.open(QIODevice::WriteOnly)) {
        m_dfuLog->appendPlainText(
            QStringLiteral("[错误] 无法写入 %1\n").arg(savePath));
        reply->deleteLater();
        return;
    }
    file.write(data);
    file.close();

    m_dfuLog->appendPlainText(
        QString::fromUtf8("[OK] dfu-util 已下载到 %1 (%2 KB)\n")
            .arg(savePath).arg(data.size() / 1024));

    m_dfuDownloadBtn->setVisible(false);
    m_dfuToolPath = savePath;
    reply->deleteLater();
}

void MainWindow::onToggleLanguage()
{
    m_langEn = !m_langEn;
    applyLanguage();
}

/* ---- 更新所有 UI 文本 (中/英切换) ---- */
void MainWindow::applyLanguage()
{
    // 按钮
    m_btnConnect->setText(trC(QStringLiteral("连接"),
                              QStringLiteral("Connect")));
    m_btnDisconnect->setText(trC(QStringLiteral("断开"),
                                QStringLiteral("Disconnect")));
    m_btnLang->setText(m_langEn ? QStringLiteral("中") : QStringLiteral("EN"));
    m_btnLang->setToolTip(m_langEn
        ? QStringLiteral("切换到中文") : QStringLiteral("Switch to English"));

    m_btnRead->setText(trC(QStringLiteral("从设备读取"),
                           QStringLiteral("Read from Device")));
    m_btnWrite->setText(trC(QStringLiteral("写入设备"),
                            QStringLiteral("Write to Device")));
    m_btnReset->setText(trC(QStringLiteral("恢复默认"),
                            QStringLiteral("Reset Defaults")));
    m_btnDfu->setText(trC(QString::fromUtf8("固件升级"),
                          QStringLiteral("Firmware")));

    // 状态
    updateConnectionUI(m_connected);

    // SEGA tab
    m_sega_tn32->setText(trC(
        QString::fromUtf8("启用 TN32 模拟。默认：模拟 15693 读卡器"),
        QStringLiteral("Enable TN32 emulation (default: emulate 15693 reader)")));
    m_sega_aic->setText(trC(
        QString::fromUtf8("FeliCa AIC 卡号转换（转为真实序列号）"),
        QStringLiteral("FeliCa AIC transform to real serial")));
    m_sega_nesica->setText(trC(
        QString::fromUtf8("启用 NESiCA 支持（MIFARE Ultralight）"),
        QStringLiteral("Enable NESiCA (MIFARE Ultralight)")));
    m_sega_ea_pass->setText(trC(
        QString::fromUtf8("启用 e-Amusement Pass 支持（ISO 15693）"),
        QStringLiteral("Enable e-Amusement Pass (ISO 15693)")));
    m_sega_tunion->setText(trC(
        QString::fromUtf8("启用交通联合支持（ISO 14443-A，APDU）"),
        QStringLiteral("Enable T-Union (ISO 14443-A, APDU)")));
    m_sega_jubeat->setText(trC(
        QString::fromUtf8("启用乐动魔方支持（Jubeat China，MIFARE）"),
        QStringLiteral("Enable Jubeat China (MIFARE)")));

    // SPICE tab
    m_spice_2p->setText(trC(
        QString::fromUtf8("启用 2P 模式"),
        QStringLiteral("Enable 2P mode")));
    m_spice_aime->setText(trC(
        QString::fromUtf8("启用 Aime 支持（MIFARE Classic）"),
        QStringLiteral("Enable Aime (MIFARE Classic)")));
    m_spice_banapass->setText(trC(
        QString::fromUtf8("启用 Banapass 支持（MIFARE Classic）"),
        QStringLiteral("Enable Banapass (MIFARE Classic)")));
    m_spice_nesica->setText(trC(
        QString::fromUtf8("启用 NESiCA 支持（MIFARE Ultralight）"),
        QStringLiteral("Enable NESiCA (MIFARE Ultralight)")));
    m_spice_tunion->setText(trC(
        QString::fromUtf8("启用交通联合支持（ISO 14443-A，APDU）"),
        QStringLiteral("Enable T-Union (ISO 14443-A, APDU)")));
    m_spice_jubeat->setText(trC(
        QString::fromUtf8("启用乐动魔方支持（Jubeat China，MIFARE）"),
        QStringLiteral("Enable Jubeat China (MIFARE)")));

    // NAMCO tab
    m_namco_todo->setText(trC(
        QStringLiteral("（保留）"), QStringLiteral("(Reserved)")));
    m_namco_aime->setText(trC(
        QString::fromUtf8("启用 Aime 支持（MIFARE Classic）"),
        QStringLiteral("Enable Aime (MIFARE Classic)")));
    m_namco_banapass->setText(trC(
        QString::fromUtf8("启用 Banapass 支持（MIFARE Classic）"),
        QStringLiteral("Enable Banapass (MIFARE Classic)")));
    m_namco_nesica->setText(trC(
        QString::fromUtf8("启用 NESiCA 支持（MIFARE Ultralight）"),
        QStringLiteral("Enable NESiCA (MIFARE Ultralight)")));
    m_namco_tunion->setText(trC(
        QString::fromUtf8("启用交通联合支持（ISO 14443-B）"),
        QStringLiteral("Enable T-Union (ISO 14443-B)")));
    m_namco_jubeat->setText(trC(
        QString::fromUtf8("启用乐动魔方支持（Jubeat China，MIFARE）"),
        QStringLiteral("Enable Jubeat China (MIFARE)")));

    // System tab group titles
    if (m_cmbUartBaud) {
        auto *p = m_cmbUartBaud->parentWidget();
        while (p) {
            auto *gb = qobject_cast<QGroupBox*>(p);
            if (gb) {
                QString title = gb->title();
                if (title.contains(QString::fromUtf8("波特率")) ||
                    title.contains("Baud") || title.contains("UART"))
                    gb->setTitle(trC(QString::fromUtf8("串口波特率"),
                                    QStringLiteral("UART Baudrate")));
                else if (title.contains("LED") || title.contains(QString::fromUtf8("亮度")))
                    gb->setTitle(trC(QString::fromUtf8("LED 全局亮度"),
                                    QStringLiteral("Global LED Brightness")));
                break;
            }
            p = p->parentWidget();
        }
    }

    // 卡片区域
    if (!m_connected || !m_lblCardUid->property("hasCard").toBool())
        clearCardDisplay();

    // 状态栏
    m_lblStatusMsg->setText(trC(
        QStringLiteral("就绪 — 点击连接开始"),
        QStringLiteral("Ready — Click Connect to start")));
}

// ====================================================================
//  HIDWorker 信号响应
// ====================================================================

void MainWindow::onDeviceConnected(const QString &desc)
{
    m_connected = true;
    updateConnectionUI(true);
    setControlsEnabled(true);
    m_btnConnect->setEnabled(true);
    m_lblStatusMsg->setText(trC(
        QStringLiteral("已连接: %1  |  正在读取设置...").arg(desc),
        QStringLiteral("Connected: %1  |  Reading settings...").arg(desc)));
    armFlashReadTimeout();          // 启动读取超时看门狗
    m_hid->requestFlashRead();
}

void MainWindow::onDeviceDisconnected()
{
    disarmFlashReadTimeout();
    m_connected = false;
    m_btnConnect->setEnabled(true);
    updateConnectionUI(false);
    clearCardDisplay();
    m_lblFwVersion->setText(QStringLiteral("FW: --"));
    m_lblReaderModel->hide();

    if (m_inDfuMode) {
        m_inDfuMode = false;
        setControlsEnabled(false);
        m_stackedRight->setCurrentIndex(1);
        m_lblStatusMsg->setText(trC(
            QString::fromUtf8("设备已进入 DFU 模式，请选择固件文件烧录。"),
            QStringLiteral("Device entered DFU mode. Select firmware to flash.")));
        onDfuCheckTool();   // 检查 dfu-util 是否可用
    } else {
        setControlsEnabled(false);
        m_lblStatusMsg->setText(trC(
            QStringLiteral("设备已断开"),
            QStringLiteral("Device disconnected")));
    }
}

void MainWindow::onDeviceError(const QString &msg)
{
    disarmFlashReadTimeout();
    m_btnConnect->setEnabled(true);
    QMessageBox::warning(this,
        trC(QStringLiteral("设备错误"), QStringLiteral("Device Error")), msg);
    m_lblStatusMsg->setText(trC(
        QStringLiteral("错误: ") + msg,
        QStringLiteral("Error: ") + msg));
}

void MainWindow::onCardDetected(uint8_t cardType, const QByteArray &rawData)
{
    m_lastCardType = cardType;

    // 写卡模式下拦截空白卡检测
    if (m_writeCardPending && cardType == Card_Blank_Mifare) {
        if (m_writeCardTimer) m_writeCardTimer->stop();
        if (m_writeCardDialog) m_writeCardDialog->accept();
        doWriteBlock2();
        return;
    }

    if (cardType == Card_None) {
        clearCardDisplay();
        m_lblStatusMsg->setText(trC(
            QString::fromUtf8("未检测到卡片"),
            QStringLiteral("No card detected")));
        return;
    }

    QString uid        = extractUid(cardType, rawData);
    QString typeName   = cardTypeName(cardType);
    QString cardNumber = extractCardNumber(cardType, rawData);

    /* AIC 卡额外显示一行: IDM 直接转十进制 */
    QString cardNumber2;
    if (cardType == Card_AIC && rawData.size() >= 8)
        cardNumber2 = idmToDecimal(rawData.left(8));

    showCardInDisplay(uid, typeName, cardNumber, cardNumber2);
    m_lblStatusMsg->setText(
        QStringLiteral("Card: %1  |  %2  |  %3 bytes")
            .arg(uid, typeName).arg(rawData.size()));
}

void MainWindow::onFlashDataRead(const FlashData &data,
                                 uint8_t readerType, uint8_t fwVersion)
{
    disarmFlashReadTimeout();       // 收到响应, 取消超时看门狗
    m_flashData = data;
    m_fwVersion = fwVersion;
    populateSettingsUI(data);

    int fwMajor = fwVersion >> 4;
    int fwMinor = fwVersion & 0x0F;
    m_lblFwVersion->setText(
        QStringLiteral("FW: v%1.%2").arg(fwMajor).arg(fwMinor));

    // 固件 < v0.5 不支持写卡
    m_btnWriteCardTab->setEnabled(fwVersion >= 0x05);
    if (fwVersion < 0x05) {
        m_btnWriteCardTab->setToolTip(
            QString::fromUtf8("写卡功能需要固件 v0.5 或更高 (当前 v%1.%2)")
                .arg(fwMajor).arg(fwMinor));
    }

    QString modelStr;
    switch (readerType) {
        case Reader_OpenSource: modelStr = QStringLiteral("Monica OpenSource"); break;
        case Reader_Lite:       modelStr = QStringLiteral("Monica Lite");       break;
        case Reader_Standard:   modelStr = QStringLiteral("Monica Standard");   break;
        case Reader_Pro:        modelStr = QStringLiteral("Monica Pro");        break;
        default:                modelStr = QStringLiteral("Monica (0x%1)")
                                              .arg(readerType, 2, 16, QLatin1Char('0'));
    }
    m_lblReaderModel->setText(modelStr);
    m_lblReaderModel->show();

    m_lblStatusMsg->setText(
        QStringLiteral("Settings loaded  |  %1, FW v%2.%3")
            .arg(modelStr).arg(fwMajor).arg(fwMinor));
}

void MainWindow::onFlashDataWritten(bool ok)
{
    if (ok) {
        m_flashData = collectSettingsFromUI();
        m_lblStatusMsg->setText(trC(
            QStringLiteral("设置已成功写入设备"),
            QStringLiteral("Settings written to device successfully")));
        QMessageBox::information(this,
            trC(QStringLiteral("成功"), QStringLiteral("Success")),
            trC(QStringLiteral("配置已写入设备。"),
                QStringLiteral("Configuration has been written to the device.")));
    } else {
        m_lblStatusMsg->setText(trC(
            QStringLiteral("写入失败"),
            QStringLiteral("Failed to write settings")));
        QMessageBox::critical(this,
            trC(QStringLiteral("错误"), QStringLiteral("Error")),
            trC(QStringLiteral("写入设备失败。"),
                QStringLiteral("Failed to write configuration to device.")));
    }
}

// ====================================================================
//  样式表 (与之前一致, 加 lang 按钮)
// ====================================================================

/* ---- 写卡 ---- */

void MainWindow::onWriteCardClicked()
{
    if (!m_connected) {
        QMessageBox::warning(this,
            trC(QString::fromUtf8("未连接"), QStringLiteral("Not connected")),
            trC(QString::fromUtf8("请先连接读卡器再操作。"),
                QStringLiteral("Please connect the reader first.")));
        return;
    }

    QString text = m_writeCardInput->text().trimmed();

    // 二次验证
    if (text.length() != 20) {
        QMessageBox::warning(this,
            QString::fromUtf8("格式错误"),
            QString::fromUtf8("卡号必须为 20 位纯数字。"));
        return;
    }
    for (const QChar &ch : text) {
        if (!ch.isDigit()) {
            QMessageBox::warning(this,
                QString::fromUtf8("格式错误"),
                QString::fromUtf8("卡号必须为纯数字，不能包含字母或符号。"));
            return;
        }
    }
    if (text.startsWith(QLatin1Char('3'))) {
        QMessageBox::warning(this,
            QString::fromUtf8("格式错误"),
            QString::fromUtf8("卡号不能以 3 开头。3 开头为 Banapass 卡。"));
        return;
    }

    // 最终确认
    auto confirm = QMessageBox::warning(this,
        QString::fromUtf8("确认写入"),
        QString::fromUtf8(
            "即将写入卡号:\n\n%1\n\n"
            "⚠ 此操作不可逆！\n"
            "确认卡号无误且卡片为空白 MIFARE 卡？")
            .arg(text),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

    if (confirm != QMessageBox::Yes) return;

    // 准备写入数据: 10 字节 HEX
    m_writeCardSerial.clear();
    for (int i = 0; i < 20; i += 2) {
        bool ok;
        uint8_t byte = text.mid(i, 2).toUInt(&ok, 16);
        if (!ok) {
            QMessageBox::warning(this,
                QString::fromUtf8("格式错误"),
                QString::fromUtf8("卡号格式异常。"));
            return;
        }
        m_writeCardSerial.append(static_cast<char>(byte));
    }

    // 弹出等待对话框
    if (m_writeCardDialog) delete m_writeCardDialog;
    m_writeCardDialog = new QDialog(this,
        Qt::Dialog | Qt::CustomizeWindowHint | Qt::WindowTitleHint);
    m_writeCardDialog->setWindowTitle(
        QString::fromUtf8("等待空白卡片"));
    m_writeCardDialog->setMinimumWidth(320);

    QVBoxLayout *dlgLayout = new QVBoxLayout(m_writeCardDialog);
    QLabel *dlgLabel = new QLabel(
        QString::fromUtf8("请将空白 MIFARE 卡放在读卡器上...\n\n"
        "检测到空白卡后将自动写入。"), m_writeCardDialog);
    dlgLabel->setWordWrap(true);
    dlgLabel->setAlignment(Qt::AlignCenter);
    dlgLabel->setStyleSheet(QStringLiteral("font-size: 14px; padding: 20px;"));

    QPushButton *cancelBtn = new QPushButton(
        QString::fromUtf8("取消"), m_writeCardDialog);
    cancelBtn->setObjectName(QStringLiteral("btnSecondary"));
    connect(cancelBtn, &QPushButton::clicked,
            this, &MainWindow::onWriteCardCancel);

    dlgLayout->addWidget(dlgLabel);
    dlgLayout->addWidget(cancelBtn, 0, Qt::AlignCenter);

    m_writeCardStep    = 0;
    m_writeCardPending = true;

    // 超时定时器 (30 秒)
    if (!m_writeCardTimer) {
        m_writeCardTimer = new QTimer(this);
        m_writeCardTimer->setSingleShot(true);
        connect(m_writeCardTimer, &QTimer::timeout,
                this, &MainWindow::onWriteCardTimeout);
    }
    m_writeCardTimer->start(30000);

    // 卡片已在读卡器上 → 直接触发写入
    if (m_lastCardType == Card_Blank_Mifare) {
        m_writeCardTimer->stop();
        m_writeCardDialog->accept();
        doWriteBlock2();
        return;
    }

    m_writeCardDialog->exec();

    // 对话框关闭 (被 cancel 或 accept 关闭)
    if (m_writeCardPending) {
        m_writeCardPending = false;
        m_writeCardTimer->stop();
    }
}

void MainWindow::onWriteCardCancel()
{
    m_writeCardPending = false;
    if (m_writeCardTimer) m_writeCardTimer->stop();
    if (m_writeCardDialog) m_writeCardDialog->reject();
}

void MainWindow::onWriteCardResult(uint8_t status)
{
    if (m_writeCardTimer) m_writeCardTimer->stop();

    if (m_writeCardStep == 0) {
        // Block 2 写完 → 继续写 Block 3
        if (status == 0x00) {
            m_writeCardStep = 1;
            doWriteBlock3();
        } else {
            m_writeCardPending = false;
            showWriteCardError(status);
        }
    } else if (m_writeCardStep == 99) {
        // 高级写卡模式 → 单步结果
        m_writeCardPending = false;
        if (status == 0x00) {
            QMessageBox::information(this,
                QString::fromUtf8("写入成功"),
                QString::fromUtf8("高级写卡操作成功完成。"));
        } else {
            showWriteCardAdvError(status);
        }
    } else {
        // Block 3 写完 → 最终结果
        m_writeCardPending = false;
        if (status == 0x00) {
            QMessageBox::information(this,
                QString::fromUtf8("写入成功"),
                QString::fromUtf8("卡号与密钥已成功写入！\n\n"
                "卡片已变为普通 Aime 卡，可正常使用。"));
        } else {
            QMessageBox::warning(this,
                QString::fromUtf8("部分写入失败"),
                QString::fromUtf8("卡号已写入，但密钥区写入失败。\n"
                "请重试或更换空白卡。"));
        }
    }
}

void MainWindow::showWriteCardError(uint8_t status)
{
    switch (status) {
    case 0x01:
        QMessageBox::warning(this,
            QString::fromUtf8("写入失败"),
            QString::fromUtf8("未找到卡片。请确保空白 MIFARE 卡已放置好。"));
        break;
    case 0x02:
        QMessageBox::warning(this,
            QString::fromUtf8("写入失败"),
            QString::fromUtf8("检测到的卡片与操作前不一致。请重新放置同一张空白卡。"));
        break;
    case 0x03:
    case 0x04:
        QMessageBox::warning(this,
            QString::fromUtf8("写入失败"),
            QString::fromUtf8("卡片密钥不匹配 (KeyA/KeyB)。请使用厂制空白 MIFARE 卡。"));
        break;
    case 0x05:
        QMessageBox::critical(this,
            QString::fromUtf8("写入失败"),
            QString::fromUtf8("数据写入失败。可能是卡片损坏或放置不稳定。"));
        break;
    default:
        QMessageBox::critical(this,
            QString::fromUtf8("写入失败"),
            QString::fromUtf8("读卡器返回未知错误 (0x%1)。")
                .arg(status, 2, 16, QLatin1Char('0')));
        break;
    }
}

void MainWindow::showWriteCardAdvError(uint8_t status)
{
    switch (status) {
    case 0x01:
        QMessageBox::warning(this,
            QString::fromUtf8("写入失败"),
            QString::fromUtf8("读卡器未检测到任何卡片。\n请确认卡片已放置好。"));
        break;
    case 0x02:
        QMessageBox::warning(this,
            QString::fromUtf8("写入失败"),
            QString::fromUtf8("卡片在操作过程中被移走。\n请保持卡片稳定。"));
        break;
    case 0x03:
    case 0x04:
        QMessageBox::warning(this,
            QString::fromUtf8("写入失败"),
            QString::fromUtf8("卡片密钥验证失败 (KeyA/KeyB)。\n请检查输入的密钥是否正确。"));
        break;
    case 0x05:
        QMessageBox::critical(this,
            QString::fromUtf8("写入失败"),
            QString::fromUtf8("数据写入失败。\n可能是卡片损坏或区块已锁定。"));
        break;
    default:
        QMessageBox::critical(this,
            QString::fromUtf8("写入失败"),
            QString::fromUtf8("读卡器返回未知错误 (0x%1)。")
                .arg(status, 2, 16, QLatin1Char('0')));
        break;
    }
}

void MainWindow::onWriteCardTimeout()
{
    if (!m_writeCardPending) return;
    m_writeCardPending = false;
    m_writeCardStep = 0;
    if (m_writeCardDialog) m_writeCardDialog->reject();

    QMessageBox::warning(this,
        QString::fromUtf8("读卡器无响应"),
        QString::fromUtf8("等待超时 (30 秒)，未检测到空白卡片。"));
}

void MainWindow::doWriteBlock2()
{
    // 数据 = 6 零 + 10 序列号
    QByteArray data(16, '\0');
    memcpy(data.data() + 6, m_writeCardSerial.constData(), 10);

    QByteArray blankKey(6, '\xFF');

    QMetaObject::invokeMethod(m_hid, "requestWriteCard",
        Qt::QueuedConnection,
        Q_ARG(uint8_t,   (uint8_t)0x00),   // sector 0
        Q_ARG(uint8_t,   (uint8_t)0x02),   // block 2
        Q_ARG(QByteArray, blankKey),
        Q_ARG(QByteArray, blankKey),
        Q_ARG(QByteArray, data));
}

void MainWindow::doWriteBlock3()
{
    // Block 3 = 密钥区 + 访问控制
    static const uint8_t raw[16] = {
        0x57, 0x43, 0x43, 0x46, 0x76, 0x32,
        0xFF, 0x07, 0x80, 0x69,
        0x57, 0x43, 0x43, 0x46, 0x76, 0x32
    };
    QByteArray block3data(reinterpret_cast<const char*>(raw), 16);
    QByteArray blankKey(6, '\xFF');

    QMetaObject::invokeMethod(m_hid, "requestWriteCard",
        Qt::QueuedConnection,
        Q_ARG(uint8_t,   (uint8_t)0x00),   // sector 0
        Q_ARG(uint8_t,   (uint8_t)0x03),   // block 3
        Q_ARG(QByteArray, blankKey),
        Q_ARG(QByteArray, blankKey),
        Q_ARG(QByteArray, block3data));
}

static QByteArray hexToBytes(const QString &hex, int expectedLen, bool &ok)
{
    ok = true;
    QString trimmed = hex.trimmed();
    if (trimmed.length() != expectedLen * 2) { ok = false; return {}; }
    for (const QChar &ch : trimmed) {
        if (!ch.isDigit() && !(ch >= QLatin1Char('A') && ch <= QLatin1Char('F'))
                         && !(ch >= QLatin1Char('a') && ch <= QLatin1Char('f')))
        { ok = false; return {}; }
    }
    return QByteArray::fromHex(trimmed.toLatin1());
}

void MainWindow::doWriteAdv()
{
    if (!m_connected) {
        QMessageBox::warning(this,
            QString::fromUtf8("未连接"), QString::fromUtf8("请先连接读卡器。"));
        return;
    }

    // 解析扇区
    bool ok;
    int sector = m_advSector->text().trimmed().toInt(&ok);
    if (!ok || sector < 0 || sector > 15) {
        QMessageBox::warning(this, QString::fromUtf8("参数错误"),
            QString::fromUtf8("扇区必须为 0-15 之间的整数。"));
        return;
    }
    int block = m_advBlock->text().trimmed().toInt(&ok);
    if (!ok || block < 0 || block > 3) {
        QMessageBox::warning(this, QString::fromUtf8("参数错误"),
            QString::fromUtf8("区块必须为 0-3 之间的整数。"));
        return;
    }

    // 解析 HEX
    QByteArray keyA = hexToBytes(m_advKeyA->text(), 6, ok);
    if (!ok) {
        QMessageBox::warning(this, QString::fromUtf8("参数错误"),
            QString::fromUtf8("KeyA 必须为 12 位十六进制数。"));
        return;
    }
    QByteArray keyB = hexToBytes(m_advKeyB->text(), 6, ok);
    if (!ok) {
        QMessageBox::warning(this, QString::fromUtf8("参数错误"),
            QString::fromUtf8("KeyB 必须为 12 位十六进制数。"));
        return;
    }
    QByteArray data = hexToBytes(m_advData->text(), 16, ok);
    if (!ok) {
        QMessageBox::warning(this, QString::fromUtf8("参数错误"),
            QString::fromUtf8("数据必须为 32 位十六进制数。"));
        return;
    }

    auto confirm = QMessageBox::warning(this,
        QString::fromUtf8("确认高级写入"),
        QString::fromUtf8(
            "即将执行高级写卡:\n"
            "扇区=%1  区块=%2\n"
            "KeyA=%3  KeyB=%4\n"
            "数据=%5\n\n"
            "⚠ 操作失误可能导致卡片写死！")
            .arg(sector).arg(block)
            .arg(QString::fromLatin1(keyA.toHex().toUpper()))
            .arg(QString::fromLatin1(keyB.toHex().toUpper()))
            .arg(QString::fromLatin1(data.toHex().toUpper())),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (confirm != QMessageBox::Yes) return;

    // 直接发送, 不等待空白卡
    QMetaObject::invokeMethod(m_hid, "requestWriteCard",
        Qt::QueuedConnection,
        Q_ARG(uint8_t, (uint8_t)sector),
        Q_ARG(uint8_t, (uint8_t)block),
        Q_ARG(QByteArray, keyA),
        Q_ARG(QByteArray, keyB),
        Q_ARG(QByteArray, data));

    // 等待结果 (简单超时)
    m_writeCardStep = 99;  // 高级模式, 不触发两步流程
    m_writeCardPending = true;
    if (!m_writeCardTimer) {
        m_writeCardTimer = new QTimer(this);
        m_writeCardTimer->setSingleShot(true);
        connect(m_writeCardTimer, &QTimer::timeout,
                this, &MainWindow::onWriteCardTimeout);
    }
    m_writeCardTimer->start(10000);
}

void MainWindow::applyStyleSheet()
{
    const QString qss = QStringLiteral(R"(
        * {
            font-family: "Segoe UI", "Microsoft YaHei", "PingFang SC", sans-serif;
            font-size: 13px;
        }
        QMainWindow { background-color: #F3F3F3; }

        QFrame#headerPanel {
            background-color: #FFFFFF;
            border-bottom: 1px solid #E0E0E0;
        }
        QLabel#appTitle {
            font-size: 18px; font-weight: 600; color: #1A1A1A; padding: 0 8px;
        }
        QPushButton#btnTutorial {
            background: transparent; color: #0078D4; border: none;
            font-size: 12px; text-decoration: underline; padding: 2px 8px;
        }
        QPushButton#btnTutorial:hover { color: #106EBE; }
        QLabel#connDot {
            border-radius: 5px; min-width: 10px; min-height: 10px;
            max-width: 10px; max-height: 10px;
        }
        QLabel#connDot[status="connected"]    { background-color: #16C60C; }
        QLabel#connDot[status="disconnected"] { background-color: #CCCCCC; }
        QLabel#connStatus { font-size: 12px; color: #616161; padding-left: 6px; }
        QLabel#fwVersion   { font-size: 12px; color: #0078D4; font-weight: 500; }
        QLabel#readerModel { font-size: 12px; color: #616161; padding-right: 8px; }

        QPushButton#btnLang {
            background-color: #F3F3F3; color: #616161;
            border: 1px solid #D0D0D0; border-radius: 4px;
            font-size: 12px; font-weight: 600; padding: 2px 0;
        }
        QPushButton#btnLang:hover { background-color: #E8E8E8; }

        QPushButton#btnDfu {
            background-color: #FF8C00; color: #FFFFFF; border: none;
            border-radius: 4px; padding: 6px 16px; font-weight: 600; min-height: 30px;
        }
        QPushButton#btnDfu:hover { background-color: #E67E00; }

        QPushButton#btnConnect {
            background-color: #0078D4; color: #FFFFFF; border: none;
            border-radius: 4px; padding: 6px 20px; font-weight: 500; min-height: 30px;
        }
        QPushButton#btnConnect:hover  { background-color: #106EBE; }
        QPushButton#btnConnect:pressed { background-color: #005A9E; }
        QPushButton#btnConnect:disabled { background-color: #CCCCCC; color: #999; }

        QPushButton#btnDisconnect {
            background-color: #D13438; color: #FFFFFF; border: none;
            border-radius: 4px; padding: 6px 20px; font-weight: 500; min-height: 30px;
        }
        QPushButton#btnDisconnect:hover { background-color: #C42B2F; }

        QWidget#cardMonitorPanel { background-color: #FAFAFA; }
        QFrame#cardDisplay {
            background-color: #1E1E2E; border-radius: 12px; border: 1px solid #2D2D44;
        }
        QLabel#cardUidTitle {
            color: #7C7C9A; font-size: 11px; font-weight: 600; letter-spacing: 2px;
        }
        QLabel#cardUidValue {
            color: #9090A0; font-family: "Cascadia Code", "Consolas", monospace;
            font-size: 13px; font-weight: 400; letter-spacing: 1px; padding: 2px 0;
        }
        QLabel#cardUidValue[hasCard="true"] { color: #9090A0; }
        QLabel#cardNumberLabel {
            color: #FFD54F; font-family: "Cascadia Code", "Consolas", monospace;
            font-size: 18px; font-weight: 700; letter-spacing: 1px; padding: 4px 0;
        }
        QLabel#cardNumberLabel[placeholder="true"] {
            color: #4A4A6A; font-weight: 400;
        }
        QLabel#cardNumberLabel2 {
            color: #82B1FF; font-family: "Cascadia Code", "Consolas", monospace;
            font-size: 14px; font-weight: 500; letter-spacing: 1px; padding: 0 0 2px;
        }
        QLabel#cardTypeLabel { color: #82B1FF; font-size: 12px; font-weight: 600; }

        QLabel#sectionTitle {
            font-size: 11px; font-weight: 700; color: #8A8A8A; letter-spacing: 2px; padding: 4px 0;
        }

        /* === Raw Data Log === */
        QTextEdit#rawLog {
            background-color: #1A1A2E;
            color: #CCCCCC;
            font-family: "Cascadia Code", "Consolas", monospace;
            font-size: 12px;
            border: 1px solid #2D2D44;
            border-radius: 6px;
            padding: 8px;
            selection-background-color: #333355;
        }

        QPushButton#btnSecondary {
            background-color: #FFFFFF; color: #616161; border: 1px solid #D0D0D0;
            border-radius: 4px; padding: 4px 16px; font-size: 12px;
        }
        QPushButton#btnSecondary:hover { background-color: #F3F3F3; border-color: #B0B0B0; }

        QWidget#settingsPanel { background-color: #FFFFFF; }
        QTabWidget#settingsTabs::pane {
            border: 1px solid #E0E0E0; border-radius: 8px; background-color: #FFFFFF;
        }
        QTabBar::tab {
            background: transparent; color: #616161; padding: 8px 22px;
            border: none; border-bottom: 2px solid transparent; margin-right: 2px;
            font-size: 13px; font-weight: 500;
        }
        QTabBar::tab:selected { color: #0078D4; border-bottom: 2px solid #0078D4; }
        QTabBar::tab:hover:!selected {
            color: #323130; background-color: rgba(0,0,0,0.03); border-radius: 4px 4px 0 0;
        }

        QLabel#tabDesc { color: #8A8A8A; font-size: 12px; line-height: 1.6; }
        QCheckBox#settingCheck { spacing: 10px; color: #323130; font-size: 13px; padding: 3px 0; }
        QCheckBox#settingCheck::indicator {
            width: 18px; height: 18px; border: 2px solid #A0A0A0;
            border-radius: 3px; background-color: #FFFFFF;
        }
        QCheckBox#settingCheck::indicator:checked {
            background-color: #0078D4; border-color: #0078D4;
        }
        QCheckBox#settingCheck::indicator:hover { border-color: #0078D4; }

        QGroupBox#sysGroup {
            background-color: #FAFAFA; border: 1px solid #E8E8E8;
            border-radius: 6px; margin-top: 8px; padding: 16px 20px;
            font-weight: 600; color: #323130;
        }
        QGroupBox#sysGroup::title { subcontrol-origin: margin; left: 16px; padding: 0 6px; }

        QComboBox#sysCombo {
            border: 1px solid #C0C0C0; border-radius: 4px; padding: 6px 12px;
            background-color: #FFFFFF; color: #323130; min-height: 30px;
        }
        QComboBox#sysCombo:hover { border-color: #0078D4; }
        QComboBox#sysCombo::drop-down { border: none; width: 28px; }

        QSlider#ledSlider::groove:horizontal {
            height: 4px; background: #E0E0E0; border-radius: 2px;
        }
        QSlider#ledSlider::handle:horizontal {
            width: 16px; height: 16px; background: #0078D4;
            border-radius: 8px; margin: -6px 0;
        }
        QSlider#ledSlider::handle:horizontal:hover { background: #106EBE; }

        QLabel#ledValue {
            color: #323130; font-size: 13px; font-weight: 600;
        }
        QLabel#sysHint { color: #A0A0A0; font-size: 11px; padding-left: 4px; }

        QPushButton#btnPrimary {
            background-color: #0078D4; color: #FFFFFF; border: none;
            border-radius: 4px; padding: 8px 24px; font-weight: 500; min-height: 34px;
        }
        QPushButton#btnPrimary:hover  { background-color: #106EBE; }
        QPushButton#btnPrimary:pressed { background-color: #005A9E; }
        QPushButton#btnPrimary:disabled { background-color: #CCCCCC; color: #999999; }

        QStatusBar#appStatusBar {
            background-color: #FFFFFF; border-top: 1px solid #E0E0E0; padding: 4px 12px;
        }
        QLabel#statusMsg { color: #616161; font-size: 12px; }
        QLabel#statusVersion { color: #9E9E9E; font-size: 11px; padding-right: 4px; }
    )");

    qApp->setStyleSheet(qss);
}
