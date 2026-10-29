#include "portscan.h"

#include <QtSerialPort/QSerialPortInfo>

#include <QStringList>
#include <QRegularExpression>

#include <algorithm>

#ifdef Q_OS_WIN
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#  include <setupapi.h>
#endif

/* --------------------------------------------------------------------
 *  ComPortInfo
 * -------------------------------------------------------------------- */

QString ComPortInfo::displayName() const
{
    if (description.isEmpty())
        return portName;
    /* 部分驱动的 description 本身就带 "(COMx)", 避免重复 */
    if (description.contains(portName, Qt::CaseInsensitive))
        return description;
    return QStringLiteral("%1 (%2)").arg(description, portName);
}

/* --------------------------------------------------------------------
 *  PortScan
 * -------------------------------------------------------------------- */

namespace
{

/* COM 号自然排序: COM2 < COM10 */
int comNumber(const QString &portName)
{
    static const QRegularExpression re(QStringLiteral("(\\d+)$"));
    const auto m = re.match(portName);
    return m.hasMatch() ? m.captured(1).toInt() : 0;
}

bool portLessThan(const ComPortInfo &a, const ComPortInfo &b)
{
    return comNumber(a.portName) < comNumber(b.portName);
}

} // namespace

namespace PortScan
{

QList<ComPortInfo> listComPorts()
{
    QList<ComPortInfo> result;

    const auto ports = QSerialPortInfo::availablePorts();
    result.reserve(ports.size());

    for (const QSerialPortInfo &info : ports) {
        /* 跳过没有名字的假入口 */
        if (info.portName().isEmpty())
            continue;

        ComPortInfo ci;
        ci.portName    = info.portName();
        ci.description = info.description().trimmed();

        /* 个别环境 description 为 "n/a" */
        if (ci.description.compare(QStringLiteral("n/a"), Qt::CaseInsensitive) == 0)
            ci.description.clear();

        if (info.hasVendorIdentifier() && info.hasProductIdentifier()) {
            ci.vid = info.vendorIdentifier();
            ci.pid = info.productIdentifier();
            ci.hasUsbIds = true;
        }
        result.append(ci);
    }

    std::sort(result.begin(), result.end(), portLessThan);
    return result;
}

bool findBootloaderPort(ComPortInfo *out)
{
    const auto ports = listComPorts();
    for (const ComPortInfo &p : ports) {
        if (p.isBootloader()) {
            if (out) *out = p;
            return true;
        }
    }
    return false;
}

bool isUsbPresent(quint16 vid, quint16 pid)
{
#ifdef Q_OS_WIN
    const QString needle = QStringLiteral("VID_%1&PID_%2")
        .arg(vid, 4, 16, QLatin1Char('0'))
        .arg(pid, 4, 16, QLatin1Char('0'))
        .toUpper();

    HDEVINFO devs = SetupDiGetClassDevsW(nullptr, L"USB", nullptr,
                                         DIGCF_PRESENT | DIGCF_ALLCLASSES);
    if (devs == INVALID_HANDLE_VALUE)
        return false;

    bool found = false;
    SP_DEVINFO_DATA data;
    data.cbSize = sizeof(data);

    for (DWORD i = 0; SetupDiEnumDeviceInfo(devs, i, &data); ++i) {
        /* SPDRP_HARDWAREID 是 REG_MULTI_SZ */
        DWORD type = 0;
        DWORD size = 0;
        SetupDiGetDeviceRegistryPropertyW(devs, &data, SPDRP_HARDWAREID,
                                          &type, nullptr, 0, &size);
        if (size == 0)
            continue;

        QByteArray buf(static_cast<int>(size) + 2, '\0');
        if (!SetupDiGetDeviceRegistryPropertyW(
                devs, &data, SPDRP_HARDWAREID, &type,
                reinterpret_cast<PBYTE>(buf.data()), size, &size))
            continue;

        const QStringList ids = QString::fromWCharArray(
                                    reinterpret_cast<const wchar_t *>(buf.constData()))
                                    .split(QLatin1Char('\0'), Qt::SkipEmptyParts);
        for (const QString &id : ids) {
            if (id.toUpper().contains(needle)) {
                found = true;
                break;
            }
        }
        if (found)
            break;
    }

    SetupDiDestroyDeviceInfoList(devs);
    return found;
#else
    Q_UNUSED(vid)
    Q_UNUSED(pid)
    return false;
#endif
}

QString describePorts()
{
    const auto ports = listComPorts();
    if (ports.isEmpty())
        return QStringLiteral("(未发现任何串口)");

    QStringList lines;
    for (const ComPortInfo &p : ports) {
        QString tag;
        if (p.isBootloader())
            tag = QStringLiteral("  <== Bootloader");
        lines.append(QStringLiteral("  %1%2").arg(p.displayName(), tag));
    }
    return lines.join(QLatin1Char('\n'));
}

} // namespace PortScan
