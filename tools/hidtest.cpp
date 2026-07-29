/*
 * hidtest — NFC Card Reader HID 原始数据测试工具
 *
 * 功能: 打开 Usage Page=0xAFCA 的 HID 设备,
 *       打印所有收到的 RAW 数据,
 *       支持手动输入十六进制命令并发送.
 *
 * 编译: g++ -std=c++17 -O2 hidtest.cpp -o hidtest -lhidapi
 * 运行: ./hidtest
 *
 * 命令格式 (大小写不敏感, 空格分隔):
 *   05 FF 70             — 发送 Flash Read 命令
 *   05 FF 60 80 80 80    — 发送 LED 命令 (ReportID=Cmd=R=G=B=)
 *   05 FF 71 AA BB ...   — 发送 Flash Write
 *   q / quit / exit      — 退出
 */

extern "C" {
#include <hidapi.h>
}

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>

#include <iostream>
#include <thread>
#include <atomic>
#include <string>
#include <vector>

#define VENDOR_USAGE_PAGE  0xAFCA
#define READ_BUF_SIZE      128

static std::atomic<bool> g_running{true};
static hid_device *g_dev = nullptr;

/* ---- 十六进制打印 ---- */
static void hexdump(const uint8_t *buf, int len)
{
    printf("  <-  %3d bytes: ", len);
    for (int i = 0; i < len; ++i) {
        printf("%02X ", buf[i]);
    }
    printf("\n");
}

/* ---- 读线程: 阻塞读取 HID 数据 ---- */
static void reader_thread()
{
    uint8_t buf[READ_BUF_SIZE];

    while (g_running.load(std::memory_order_relaxed)) {
        int ret = hid_read_timeout(g_dev, buf, sizeof(buf), 200);
        if (ret > 0) {
            hexdump(buf, ret);
        } else if (ret < 0) {
            printf("\n  !! Read error, device disconnected.\n");
            g_running = false;
            break;
        }
    }
}

/* ---- 解析十六进制字符串 -> 字节数组 ---- */
static bool parseHex(const std::string &line, std::vector<uint8_t> &out)
{
    out.clear();
    const char *p = line.c_str();

    while (*p) {
        // 跳过空格
        while (*p && isspace(static_cast<unsigned char>(*p))) ++p;
        if (!*p) break;

        // 读取两个十六进制字符
        char hex[3] = {0, 0, 0};
        int i = 0;
        while (i < 2 && *p && isxdigit(static_cast<unsigned char>(*p))) {
            hex[i++] = *p;
            ++p;
        }
        if (i == 0) return false;   // 非法字符

        hex[i] = '\0';
        out.push_back(static_cast<uint8_t>(strtoul(hex, nullptr, 16)));

        // 跳过可能的尾部非空白字符 (容错)
        while (*p && !isspace(static_cast<unsigned char>(*p))) ++p;
    }
    return !out.empty();
}

/* ---- 入口 ---- */
int main()
{
    printf("=== NFC Card Reader HID Test Tool ===\n");
    printf("Looking for device (Usage Page 0x%04X)...\n", VENDOR_USAGE_PAGE);

    // 初始化 HIDAPI
    if (hid_init() != 0) {
        fprintf(stderr, "ERROR: hid_init() failed\n");
        return 1;
    }

    // 枚举设备
    hid_device_info *devs = hid_enumerate(0, 0);
    hid_device_info *target = nullptr;

    if (!devs) {
        fprintf(stderr, "ERROR: No HID devices found\n");
        hid_exit();
        return 1;
    }

    printf("\n  All HID devices:\n");
    for (hid_device_info *cur = devs; cur; cur = cur->next) {
        printf("    vid=%04X pid=%04X usage=%04X:%04X  %ls\n",
               cur->vendor_id, cur->product_id,
               cur->usage_page, cur->usage,
               cur->product_string);

        if (cur->usage_page == VENDOR_USAGE_PAGE) {
            target = cur;
        }
    }

    if (!target) {
        printf("\n  Device with Usage Page 0x%04X NOT FOUND.\n", VENDOR_USAGE_PAGE);
        printf("  Is the card reader plugged in and recognized by Windows?\n");
        hid_free_enumeration(devs);
        hid_exit();
        return 1;
    }

    printf("\n  -> Opening: vid=%04X pid=%04X  %ls\n",
           target->vendor_id, target->product_id, target->product_string);

    g_dev = hid_open_path(target->path);
    hid_free_enumeration(devs);

    if (!g_dev) {
        fprintf(stderr, "ERROR: Failed to open device\n");
        hid_exit();
        return 1;
    }

    printf("  Device opened successfully!\n\n");
    printf("Commands: type hex bytes (space-separated) then Enter to send.\n");
    printf("  Example:  05 FF 70         (Flash Read)\n");
    printf("  Example:  05 FF 60 80 80 80 (LED brightness 50%%)\n");
    printf("  Type 'q' to quit.\n");
    printf("------------------------------------------------------------\n\n");

    // 启动读线程
    std::thread reader(reader_thread);

    // 主线程: 读取用户输入
    std::string line;
    while (g_running.load(std::memory_order_relaxed)) {
        printf("> ");
        fflush(stdout);

        if (!std::getline(std::cin, line)) {
            break;  // EOF
        }

        // 去掉首尾空格
        while (!line.empty() && isspace(static_cast<unsigned char>(line.front())))
            line.erase(0, 1);
        while (!line.empty() && isspace(static_cast<unsigned char>(line.back())))
            line.pop_back();

        if (line.empty()) continue;

        // 退出命令
        if (line == "q" || line == "Q" || line == "quit" || line == "exit") {
            break;
        }

        // 解析十六进制字节
        std::vector<uint8_t> bytes;
        if (!parseHex(line, bytes)) {
            printf("  Invalid hex input. Use format like: 05 FF 70\n");
            continue;
        }

        if (bytes.empty()) continue;

        // 发送
        printf("  -> %zu bytes: ", bytes.size());
        for (auto b : bytes) printf("%02X ", b);
        printf("\n");

        int ret = hid_write(g_dev, bytes.data(), bytes.size());
        if (ret < 0) {
            printf("  !! Write failed: %ls\n", hid_error(g_dev));
        } else {
            printf("  Sent %d bytes OK.\n", ret);
        }
    }

    // 清理
    g_running = false;
    if (reader.joinable()) reader.join();

    if (g_dev) {
        hid_close(g_dev);
        g_dev = nullptr;
    }
    hid_exit();

    printf("\nBye.\n");
    return 0;
}
