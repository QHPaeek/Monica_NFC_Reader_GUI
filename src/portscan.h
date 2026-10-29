#ifndef PORTSCAN_H
#define PORTSCAN_H

/* ====================================================================
 *  串口扫描 / USB 设备探测
 *
 *  - listComPorts()      枚举系统串口, 显示名与 Windows 设备管理器一致
 *                        (形如 "USB 串行设备 (COM5)")
 *  - findBootloaderPort  按 VID/PID 找到 Affine Bootloader 的 CDC 串口
 *  - isUsbPresent()      任意 USB 设备存在性判断 (用于检测 DFU 设备,
 *                        DFU 不是串口, 无法通过 QSerialPortInfo 发现)
 * ==================================================================== */

#include <QString>
#include <QList>
#include <QtGlobal>

/* Affine Uni Bootloader — STM32F072 / Monica 目标的 USB 身份 */
#define BOOT_USB_VID            0xAFF1
#define BOOT_USB_PID_BOOTLOADER 0x52B2
#define BOOT_USB_PID_APP        0x5730
#define BOOT_USB_PID_DFU        0xDF11

struct ComPortInfo
{
    QString portName;        // "COM5"
    QString description;     // 驱动友好名, 如 "USB 串行设备"
    quint16 vid = 0;
    quint16 pid = 0;
    bool    hasUsbIds = false;

    /* 设备管理器风格的完整名称: "USB 串行设备 (COM5)" */
    QString displayName() const;

    bool isBootloader() const
    {
        return hasUsbIds && vid == BOOT_USB_VID && pid == BOOT_USB_PID_BOOTLOADER;
    }
};

/* 顶部设备下拉框的一项: HID 读卡器 或 串口 */
struct DeviceEntry
{
    enum Kind { Hid, Serial };

    Kind    kind = Serial;
    QString key;          // HID: 设备 path; Serial: "COM3"
    QString display;      // 下拉框显示文本
    quint16 vid = 0;
    quint16 pid = 0;
    bool    hasIds = false;
    bool    isBootloader = false;

    bool isHid() const { return kind == Hid; }
};

namespace PortScan
{
    /* 当前系统的全部串口 (按 COM 号自然排序) */
    QList<ComPortInfo> listComPorts();

    /* 找 Affine Bootloader 的串口; 找不到返回 false */
    bool findBootloaderPort(ComPortInfo *out);

    /* 是否存在指定 VID/PID 的 USB 设备 (不限于串口) */
    bool isUsbPresent(quint16 vid, quint16 pid);

    /* 逐字打印串口列表, 供日志使用 */
    QString describePorts();

    /* 顶部下拉框用: 已识别的 HID 读卡器 + 全部串口 */
    QList<DeviceEntry> listDevices();

    /* VID/PID → 人类可读标签 ("Monica Bootloader" / "Monica Reader" / "") */
    QString deviceTag(quint16 vid, quint16 pid);
}

#endif // PORTSCAN_H
