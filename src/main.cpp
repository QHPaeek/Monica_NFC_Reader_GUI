#include <QApplication>
#include <QStyleFactory>

#include "mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    // 使用 Fusion 风格作为基础 (跨平台一致, 最接近现代扁平化风格)
    app.setStyle(QStyleFactory::create(QStringLiteral("Fusion")));

    // 高DPI 支持 (Qt6 默认启用, Qt5 需手动设置)
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    app.setAttribute(Qt::AA_EnableHighDpiScaling, true);
    app.setAttribute(Qt::AA_UseHighDpiPixmaps, true);
#endif

    // 应用名称
    app.setApplicationName(QStringLiteral("NFC Card Reader"));
    app.setApplicationVersion(QStringLiteral("1.0.0"));
    app.setOrganizationName(QStringLiteral("CardReader"));

    MainWindow window;
    window.show();

    return app.exec();
}
