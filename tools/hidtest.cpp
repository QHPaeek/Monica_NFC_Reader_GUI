/*
 * hidtest — NFC Card Reader HID 原始数据测试工具 (RAW 模式)
 *
 * 不做任何协议过滤！所有收发数据原样显示。
 *
 * 编译: g++ -std=c++17 -O2 hidtest.cpp -o hidtest -lhidapi
 * 运行: ./hidtest
 *
 * 命令: 输入十六进制字节 (空格分隔) 回车发送。
 *       q / quit / exit — 退出。
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

#define READ_BUF_SIZE      128

static std::atomic<bool> g_running{true};
static hid_device *g_dev = nullptr;

static void hexdump(const uint8_t *buf, int len)
{
    printf("  <-  %3d bytes: ", len);
    for (int i = 0; i < len; ++i)
        printf("%02X ", buf[i]);
    printf("\n");
}

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

static bool parseHex(const std::string &line, std::vector<uint8_t> &out)
{
    out.clear();
    const char *p = line.c_str();
    while (*p) {
        while (*p && isspace(static_cast<unsigned char>(*p))) ++p;
        if (!*p) break;
        char hex[3] = {0, 0, 0};
        int i = 0;
        while (i < 2 && *p && isxdigit(static_cast<unsigned char>(*p)))
            hex[i++] = *p++;
        if (i == 0) return false;
        hex[i] = '\0';
        out.push_back(static_cast<uint8_t>(strtoul(hex, nullptr, 16)));
        while (*p && !isspace(static_cast<unsigned char>(*p))) ++p;
    }
    return !out.empty();
}

int main()
{
    printf("=== NFC HID RAW Test Tool ===\n");
    printf("No protocol filtering — all bytes shown as-is.\n\n");

    if (hid_init() != 0) {
        fprintf(stderr, "ERROR: hid_init() failed\n");
        return 1;
    }

    hid_device_info *devs = hid_enumerate(0, 0);
    if (!devs) {
        fprintf(stderr, "ERROR: No HID devices\n");
        hid_exit();
        return 1;
    }

    printf("All HID devices:\n");
    hid_device_info *target = nullptr;
    for (hid_device_info *cur = devs; cur; cur = cur->next) {
        printf("  vid=%04X pid=%04X usage=%04X:%04X  %ls\n",
               cur->vendor_id, cur->product_id,
               cur->usage_page, cur->usage, cur->product_string);
        // 优先匹配 AFCA，也接受任何可疑设备
        if (cur->usage_page == 0xAFCA)
            target = cur;
    }

    // 没找到 AFCA 就用第一个 HID 设备
    if (!target) {
        target = devs;
        printf("\n  No AFCA device found. Using first HID device.\n");
    }

    printf("\nOpening: vid=%04X pid=%04X  %ls\n",
           target->vendor_id, target->product_id, target->product_string);

    g_dev = hid_open_path(target->path);
    hid_free_enumeration(devs);

    if (!g_dev) {
        fprintf(stderr, "ERROR: Failed to open device\n");
        hid_exit();
        return 1;
    }
    printf("Device opened.\n\n");

    printf("Type hex bytes and press Enter to send.\n");
    printf("  Example: 05 FF 70\n");
    printf("  q = quit\n");
    printf("----------------------------------------\n\n");

    std::thread reader(reader_thread);

    std::string line;
    while (g_running.load(std::memory_order_relaxed)) {
        printf("> ");
        fflush(stdout);
        if (!std::getline(std::cin, line)) break;

        while (!line.empty() && isspace(static_cast<unsigned char>(line.front())))
            line.erase(0, 1);
        while (!line.empty() && isspace(static_cast<unsigned char>(line.back())))
            line.pop_back();
        if (line.empty()) continue;

        if (line == "q" || line == "Q" || line == "quit" || line == "exit")
            break;

        std::vector<uint8_t> bytes;
        if (!parseHex(line, bytes)) {
            printf("  Invalid hex.\n");
            continue;
        }

        printf("  -> %zu bytes: ", bytes.size());
        for (auto b : bytes) printf("%02X ", b);
        printf("\n");

        int ret = hid_write(g_dev, bytes.data(), bytes.size());
        if (ret < 0)
            printf("  !! Write failed: %ls\n", hid_error(g_dev));
        else
            printf("  Sent %d bytes OK.\n", ret);
    }

    g_running = false;
    if (reader.joinable()) reader.join();
    if (g_dev) { hid_close(g_dev); g_dev = nullptr; }
    hid_exit();
    printf("\nBye.\n");
    return 0;
}
