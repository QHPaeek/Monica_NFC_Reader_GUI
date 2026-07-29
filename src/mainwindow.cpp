#include "mainwindow.h"

#include <QApplication>
#include <QDateTime>
#include <QHeaderView>
#include <QMessageBox>
#include <QScrollArea>
#include <QScrollBar>
#include <QStyle>
#include <QThread>

// ====================================================================
//  构造 & 析构
// ====================================================================

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_hid(new HIDWorker(nullptr))
    , m_connected(false)
    , m_langEn(false)
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
    connect(m_hid, &HIDWorker::rawDataSent,
            this, &MainWindow::onRawDataSent);
    connect(m_hid, &HIDWorker::rawDataReceived,
            this, &MainWindow::onRawDataReceived);

    setControlsEnabled(false);
    updateConnectionUI(false);
}

MainWindow::~MainWindow()
{
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
    setWindowTitle(QStringLiteral("NFC Card Reader - Configuration Tool"));
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
    splitter->addWidget(buildSettingsPanel());
    splitter->setStretchFactor(0, 2);
    splitter->setStretchFactor(1, 3);
    splitter->setSizes({380, 720});
    mainLayout->addWidget(splitter, 1);

    m_lblStatusMsg = new QLabel(QStringLiteral("Ready - Click Connect to start"));
    m_lblStatusMsg->setObjectName(QStringLiteral("statusMsg"));
    statusBar()->addWidget(m_lblStatusMsg, 1);
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

    hbox->addWidget(title);
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
    m_lblCardUid->setWordWrap(true);

    m_lblCardNumber = new QLabel(this);
    m_lblCardNumber->setObjectName(QStringLiteral("cardNumberLabel"));
    m_lblCardNumber->setAlignment(Qt::AlignCenter);
    m_lblCardNumber->setWordWrap(true);
    m_lblCardNumber->hide();

    m_lblCardType = new QLabel(this);
    m_lblCardType->setObjectName(QStringLiteral("cardTypeLabel"));
    m_lblCardType->setAlignment(Qt::AlignCenter);
    m_lblCardType->hide();

    displayLayout->addWidget(uidTitle);
    displayLayout->addWidget(m_lblCardNumber);
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
//  卡片 UID / 卡号 提取
// ====================================================================

QString MainWindow::cardTypeName(uint8_t cardType)
{
    switch (cardType) {
        case Card_Aime:          return QStringLiteral("Aime (MIFARE)");
        case Card_Bana_New:      return QStringLiteral("Banapass (New)");
        case Card_Bana_Old:      return QStringLiteral("Banapass (Old)");
        case Card_AIC:           return QStringLiteral("AIC (FeliCa)");
        case Card_EPass:         return QStringLiteral("e-Pass (ISO15693)");
        case Card_Nesica:        return QStringLiteral("NESiCA (Ultralight)");
        case Card_Tunion:        return QStringLiteral("T-Union (14443-A)");
        case Card_Jubeat:        return QStringLiteral("Jubeat (MIFARE)");
        case Card_Unknow_Mifare: return QStringLiteral("Unknown MIFARE");
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
    case Card_AIC:
        return QString();   // 加密卡号, 不显示
    case Card_EPass:
    case Card_Nesica:
    case Card_Tunion:
        // Card_RawData[1] (offset 16) 偏移0, 10字节
        if (rawData.size() < 16 + 10) return QString();
        offset = 16;
        len    = 10;
        break;
    case Card_Jubeat:
    case Card_Unknow_Mifare:
    case Card_Blank_Mifare:
        return QString();
    default:
        return QString();
    }

    if (offset + len > rawData.size()) return QString();

    // 原始字节 → HEX 字符串 (每字节 = 2 个 HEX 字符)
    QString hex;
    for (int i = 0; i < len; ++i) {
        hex.append(QStringLiteral("%1")
            .arg(static_cast<uint8_t>(rawData[offset + i]), 2, 16,
                 QLatin1Char('0')));
    }

    // 每 4 个字符插入一个空格: "1111222233334444" → "1111 2222 3333 4444"
    QString formatted;
    for (int i = 0; i < hex.length(); ++i) {
        if (i > 0 && i % 4 == 0)
            formatted.append(QLatin1Char(' '));
        formatted.append(hex.at(i));
    }
    return formatted.toUpper();
}

void MainWindow::showCardInDisplay(const QString &uid, const QString &cardType,
                                   const QString &cardNumber)
{
    // UID: 小字灰色
    m_lblCardUid->setText(uid);
    m_lblCardUid->setProperty("hasCard", true);
    m_lblCardUid->style()->unpolish(m_lblCardUid);
    m_lblCardUid->style()->polish(m_lblCardUid);

    // 卡号: 大字金色, 主要显示
    if (!cardNumber.isEmpty()) {
        m_lblCardNumber->setText(cardNumber);
        m_lblCardNumber->show();
    } else {
        m_lblCardNumber->hide();
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
    m_lblCardType->hide();
}

// ====================================================================
//  按钮点击
// ====================================================================

void MainWindow::onConnectClicked()
{
    m_lblStatusMsg->setText(trC(
        QStringLiteral("正在搜索设备..."),
        QStringLiteral("Searching for device...")));
    m_btnConnect->setEnabled(false);
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
    m_hid->requestFlashRead();
}

void MainWindow::onDeviceDisconnected()
{
    m_connected = false;
    m_btnConnect->setEnabled(true);
    updateConnectionUI(false);
    setControlsEnabled(false);
    clearCardDisplay();
    m_lblFwVersion->setText(QStringLiteral("FW: --"));
    m_lblReaderModel->hide();
    m_lblStatusMsg->setText(trC(
        QStringLiteral("设备已断开"),
        QStringLiteral("Device disconnected")));
}

void MainWindow::onDeviceError(const QString &msg)
{
    m_btnConnect->setEnabled(true);
    QMessageBox::warning(this,
        trC(QStringLiteral("设备错误"), QStringLiteral("Device Error")), msg);
    m_lblStatusMsg->setText(trC(
        QStringLiteral("错误: ") + msg,
        QStringLiteral("Error: ") + msg));
}

void MainWindow::onCardDetected(uint8_t cardType, const QByteArray &rawData)
{
    QString uid        = extractUid(cardType, rawData);
    QString typeName   = cardTypeName(cardType);
    QString cardNumber = extractCardNumber(cardType, rawData);

    showCardInDisplay(uid, typeName, cardNumber);
    m_lblStatusMsg->setText(
        QStringLiteral("Card: %1  |  %2  |  %3 bytes")
            .arg(uid, typeName).arg(rawData.size()));
}

void MainWindow::onFlashDataRead(const FlashData &data,
                                 uint8_t readerType, uint8_t fwVersion)
{
    m_flashData = data;
    populateSettingsUI(data);

    int fwMajor = fwVersion >> 4;
    int fwMinor = fwVersion & 0x0F;
    m_lblFwVersion->setText(
        QStringLiteral("FW: v%1.%2").arg(fwMajor).arg(fwMinor));

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
            font-size: 15px; font-weight: 500; letter-spacing: 1px; padding: 4px 0;
        }
        QLabel#cardUidValue[hasCard="true"] { color: #9090A0; }
        QLabel#cardNumberLabel {
            color: #FFD54F; font-family: "Cascadia Code", "Consolas", monospace;
            font-size: 22px; font-weight: 700; letter-spacing: 2px; padding: 6px 0;
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
    )");

    qApp->setStyleSheet(qss);
}
