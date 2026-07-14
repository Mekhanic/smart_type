#include "blurcontroller.hpp"
#include "rendererbridge.hpp"

#include <QFontDatabase>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QWindow>

#include "smarttype/version.hpp"
#include <iostream>
#include <unistd.h>

bool is_debug_enabled() {
    static const bool enabled = []() {
        const char* env = std::getenv("SMARTTYPE_DEBUG");
        return env && (std::string_view(env) == "1" || std::string_view(env) == "true");
    }();
    return enabled;
}

void messageHandler(QtMsgType type, const QMessageLogContext& context, const QString& msg) {
    Q_UNUSED(type);
    Q_UNUSED(context);
    if (is_debug_enabled()) {
        std::cerr << msg.toStdString() << std::endl;
    }
}

int main(int argc, char* argv[]) {
    qInstallMessageHandler(messageHandler);
    if (is_debug_enabled()) {
        std::cerr << "[SmartType UI Startup] PID: " << getpid() << "\n"
                  << "  Executable  : " << (argc > 0 ? argv[0] : "unknown") << "\n"
                  << "  Version Hash: " << smarttype::GIT_COMMIT_HASH << "\n"
                  << "  Build Time  : " << smarttype::BUILD_TIMESTAMP << std::endl;
    }

    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("SmartType UI"));
    app.setOrganizationName(QStringLiteral("SmartType"));
    app.setQuitOnLastWindowClosed(false);

    QString demoText;
    for (int i = 1; i < argc; ++i) {
        if (QString::fromLocal8Bit(argv[i]) == QStringLiteral("--demo-flash") && i + 1 < argc) {
            demoText = QString::fromLocal8Bit(argv[i + 1]);
            app.setQuitOnLastWindowClosed(true);
            break;
        }
    }

    const QStringList families = QFontDatabase::families();
    QString family = QStringLiteral("sans-serif");
    for (const QString& candidate : {QStringLiteral("SF Pro Text"), QStringLiteral("Inter")}) {
        if (families.contains(candidate, Qt::CaseInsensitive)) {
            family = candidate;
            break;
        }
    }

    BlurController blurController;
    RendererBridge bridge;
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("blurController"), &blurController);
    engine.rootContext()->setContextProperty(QStringLiteral("rendererBridge"), &bridge);
    engine.rootContext()->setContextProperty(QStringLiteral("smartTypeFontFamily"), family);

    engine.load(QUrl(QStringLiteral("qrc:/qml/qml/Production.qml")));
    if (engine.rootObjects().isEmpty()) return 1;
    bridge.setWindow(qobject_cast<QWindow*>(engine.rootObjects().first()));

    if (!demoText.isEmpty()) {
        bridge.triggerDemoFlash(demoText);
    }

    return app.exec();
}
