#include "rendererbridge.hpp"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalServer>
#include <QLocalSocket>
#include <QStandardPaths>
#include <QDebug>
#include <QTime>
#include <QTimer>
#include <QScreen>
#include <QGuiApplication>
#include <QWindow>
#include <iostream>

namespace {
QString socketPath() {
    return QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation) +
           QStringLiteral("/smarttype-ui.sock");
}

bool is_debug_enabled() {
    static const bool enabled = []() {
        const char* env = std::getenv("SMARTTYPE_DEBUG");
        return env && (std::string_view(env) == "1" || std::string_view(env) == "true");
    }();
    return enabled;
}
}

RendererBridge::RendererBridge(QObject* parent) : QObject(parent), server_(new QLocalServer(this)) {
    QLocalServer::removeServer(socketPath());
    server_->setSocketOptions(QLocalServer::UserAccessOption);
    if (!server_->listen(socketPath())) {
        qFatal("SmartType UI cannot listen on %s", qPrintable(socketPath()));
    }
    connect(server_, &QLocalServer::newConnection, this, [this]() {
        while (QLocalSocket* socket = server_->nextPendingConnection()) {
            clients_.append(socket);
            connect(socket, &QLocalSocket::readyRead, this, [this, socket]() { readSocket(socket); });
            connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
            connect(socket, &QObject::destroyed, this, [this, socket]() { clients_.removeAll(socket); });
        }
    });

    auto bus = QDBusConnection::sessionBus();
    bus.registerService(QStringLiteral("org.smarttype.UI"));
    bus.registerObject(QStringLiteral("/SmartTypeUI"), this,
                       QDBusConnection::ExportAllSlots);
}

void RendererBridge::readSocket(QLocalSocket* socket) {
    while (socket->canReadLine()) handleMessage(socket->readLine().trimmed());
}

void RendererBridge::handleMessage(const QByteArray& line) {
    qInfo().noquote() << "[LOG" << QTime::currentTime().toString("hh:mm:ss.zzz") << "] BRIDGE received message:" << line;
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(line, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) return;
    const auto object = document.object();

    if (object.value(QStringLiteral("type")).toString() == QStringLiteral("flash")) {
        QVariantMap flashEvent;
        flashEvent[QStringLiteral("id")] = object.value(QStringLiteral("id")).toInt();
        flashEvent[QStringLiteral("x")] = object.value(QStringLiteral("x")).toInt();
        flashEvent[QStringLiteral("y")] = object.value(QStringLiteral("y")).toInt();
        flashEvent[QStringLiteral("height")] = object.value(QStringLiteral("height")).toInt(20);
        flashEvent[QStringLiteral("word")] = object.value(QStringLiteral("word")).toString();
        flashes_.append(flashEvent);
        emit flashesChanged();
        return;
    }

    panelVisible_ = object.value(QStringLiteral("visible")).toBool(false);
    flashId_ = object.value(QStringLiteral("flashId")).toInt(0);
    correction_ = object.value(QStringLiteral("correction")).toString();
    candidates_.clear();
    for (const auto& value : object.value(QStringLiteral("candidates")).toArray()) {
        candidates_.append(value.toString());
    }
    selectedIndex_ = qBound(0, object.value(QStringLiteral("selected")).toInt(0),
                            qMax(0, candidates_.size() - 1));
    cursorX_ = object.value(QStringLiteral("cursorX")).toInt();
    cursorY_ = object.value(QStringLiteral("cursorY")).toInt();
    cursorHeight_ = object.value(QStringLiteral("cursorHeight")).toInt(20);
    composingWidth_ = object.value(QStringLiteral("composingWidth")).toInt();
    program_ = object.value(QStringLiteral("program")).toString();
    frontendName_ = object.value(QStringLiteral("frontendName")).toString();

    QScreen* screen = window_ ? window_->screen() : QGuiApplication::primaryScreen();
    double dpr = window_ ? window_->devicePixelRatio() : (screen ? screen->devicePixelRatio() : 1.0);
    double dpi = screen ? screen->logicalDotsPerInch() : 96.0;
    double uiScale = dpi / 96.0;
    QString scaleFactorEnv = QString::fromLocal8Bit(qgetenv("QT_SCALE_FACTOR"));
    if (scaleFactorEnv.isEmpty()) scaleFactorEnv = QStringLiteral("not set");
    QString waylandDisplay = QString::fromLocal8Bit(qgetenv("WAYLAND_DISPLAY"));
    QString xDisplay = QString::fromLocal8Bit(qgetenv("DISPLAY"));
    QString displayStr = waylandDisplay.isEmpty() ? xDisplay : waylandDisplay;

    int panelLogicalW = window_ ? window_->width() : 0;
    int panelLogicalH = window_ ? window_->height() : 0;
    int renderedBufferW = static_cast<int>(panelLogicalW * dpr);
    int renderedBufferH = static_cast<int>(panelLogicalH * dpr);
    int verticalPadding = 4;
    
    int pillW = 0;
    int pillH = 0;
    if (selectedIndex_ >= 0 && selectedIndex_ < candidates_.size()) {
        pillH = 42;
        pillW = qMax(92, static_cast<int>(candidates_[selectedIndex_].size() * 10 + 36)) - 8;
    }

    if (is_debug_enabled()) {
        std::cerr << "\n================= GEOMETRY DIAGNOSTIC LOG =================\n"
                  << "  program: " << program_.toStdString() << "\n"
                  << "  frontendName: " << frontendName_.toStdString() << "\n"
                  << "  display: " << displayStr.toStdString() << "\n"
                  << "  cursorRect: x=" << cursorX_ << " y=" << cursorY_
                  << " w=" << composingWidth_ << " h=" << cursorHeight_ << "\n"
                  << "  QScreen name: " << (screen ? screen->name().toStdString() : "unknown") << "\n"
                  << "  devicePixelRatio: " << dpr << "\n"
                  << "  logicalDotsPerInch: " << dpi << "\n"
                  << "  QT_SCALE_FACTOR: " << scaleFactorEnv.toStdString() << "\n"
                  << "  calculated uiScale: " << uiScale << "\n"
                  << "  panel logical size: " << panelLogicalW << "x" << panelLogicalH << "\n"
                  << "  rendered buffer size: " << renderedBufferW << "x" << renderedBufferH << "\n"
                  << "  candidate font pixel size: 16\n"
                  << "  verticalPadding: " << verticalPadding << "\n"
                  << "  selectionPill rect: x=estimated y=estimated w=" << pillW << " h=" << pillH << "\n"
                  << "===========================================================\n" << std::endl;
    }

    recomputePosition();
    qInfo().noquote() << "[LOG" << QTime::currentTime().toString("hh:mm:ss.zzz") << "] BRIDGE window visible=" << panelVisible_ << "flashId=" << flashId_ << "correction=" << correction_;
    emit panelChanged();
}

void RendererBridge::SetActiveWindowGeometry(int x, int y, int width, int height) {
    qInfo() << "[LOG BRIDGE] SetActiveWindowGeometry (int) called with x=" << x << "y=" << y << "w=" << width << "h=" << height;
    windowX_ = x;
    windowY_ = y;
    windowWidth_ = width;
    windowHeight_ = height;
    haveWindowGeometry_ = windowWidth_ > 0 && windowHeight_ > 0;
    recomputePosition();
}

void RendererBridge::SetActiveWindowGeometry(double x, double y, double width, double height) {
    qInfo() << "[LOG BRIDGE] SetActiveWindowGeometry (double) called with x=" << x << "y=" << y << "w=" << width << "h=" << height;
    windowX_ = static_cast<int>(x);
    windowY_ = static_cast<int>(y);
    windowWidth_ = static_cast<int>(width);
    windowHeight_ = static_cast<int>(height);
    haveWindowGeometry_ = windowWidth_ > 0 && windowHeight_ > 0;
    recomputePosition();
}

void RendererBridge::recomputePosition() {
    const int baseX = haveWindowGeometry_ ? windowX_ : 0;
    const int baseY = haveWindowGeometry_ ? windowY_ : 0;
    const int nextX = baseX + cursorX_ - composingWidth_ + qMin(72, composingWidth_ * 2 / 5);
    const int nextY = baseY + cursorY_ - 90;
    const int nextBelowY = baseY + cursorY_ + cursorHeight_ + 22;
    if (nextX == targetX_ && nextY == targetY_ && nextBelowY == targetBelowY_) return;
    targetX_ = nextX;
    targetY_ = nextY;
    targetBelowY_ = nextBelowY;
    emit positionChanged();
}

void RendererBridge::chooseCandidate(int index) {
    const QByteArray command = QByteArrayLiteral("select ") + QByteArray::number(index) + '\n';
    for (const auto& client : clients_) {
        if (client && client->state() == QLocalSocket::ConnectedState) {
            client->write(command);
            client->flush();
        }
    }
}

void RendererBridge::removeFlash(int id) {
    for (int i = 0; i < flashes_.size(); ++i) {
        if (flashes_[i].toMap().value(QStringLiteral("id")).toInt() == id) {
            flashes_.removeAt(i);
            emit flashesChanged();
            break;
        }
    }
}

void RendererBridge::triggerDemoFlash(const QString& text) {
    panelVisible_ = true;
    flashId_ = 42;
    correction_ = text;
    cursorX_ = 400;
    cursorY_ = 300;
    cursorHeight_ = 24;
    composingWidth_ = 0;
    recomputePosition();
    qInfo().noquote() << "[LOG" << QTime::currentTime().toString("hh:mm:ss.zzz") << "] BRIDGE triggerDemoFlash text=" << text;
    emit panelChanged();
    QTimer::singleShot(1000, qApp, &QCoreApplication::quit);
}
