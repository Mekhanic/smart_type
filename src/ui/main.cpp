#include "blurcontroller.hpp"

#include <QFontDatabase>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>

int main(int argc, char* argv[]) {
    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("SmartType UI Demo"));
    app.setOrganizationName(QStringLiteral("SmartType"));

    const QStringList families = QFontDatabase::families();
    QString family = QStringLiteral("sans-serif");
    for (const QString& candidate : {QStringLiteral("SF Pro Text"), QStringLiteral("Inter")}) {
        if (families.contains(candidate, Qt::CaseInsensitive)) {
            family = candidate;
            break;
        }
    }

    BlurController blurController;
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("blurController"), &blurController);
    engine.rootContext()->setContextProperty(QStringLiteral("smartTypeFontFamily"), family);
    engine.load(QUrl(QStringLiteral("qrc:/qml/qml/Main.qml")));
    if (engine.rootObjects().isEmpty()) return 1;
    return app.exec();
}
