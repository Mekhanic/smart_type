#include "smarttype/personal_store.hpp"
#include "smarttype/version.hpp"
#include "issuereportdialog.hpp"
#include "settingsdialog.hpp"

#include <iostream>
#include <unistd.h>
#include <QActionGroup>
#include <QApplication>
#include <QCursor>
#include <QDesktopServices>
#include <QEvent>
#include <QIcon>
#include <QMenu>
#include <QPalette>
#include <QProcess>
#include <QPixmap>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QUrl>
#include <QLocalServer>
#include <QLocalSocket>

#include <memory>
#include <utility>
#include <chrono>

namespace {

constexpr auto kProjectUrl = "https://github.com/Mekhanic/smart_type";

QIcon iconFromResource(const QString& path) {
    const QPixmap source(path);
    if (!source.isNull()) {
        // Build an unnamed QIcon from concrete pixmaps. QSystemTrayIcon then
        // publishes IconPixmap over StatusNotifierItem instead of an IconName
        // that Plasma may resolve to the obsolete cached smarttype.svg.
        QIcon icon;
        for (const int size : {16, 22, 32, 48, 64}) {
            icon.addPixmap(source.scaled(size, size, Qt::KeepAspectRatio,
                                         Qt::SmoothTransformation));
        }
        return icon;
    }
    QIcon icon = QIcon::fromTheme(QStringLiteral("smarttype-idle"));
    if (icon.isNull()) {
        icon = QIcon::fromTheme(QStringLiteral("accessories-character-map"));
    }
    return icon;
}

QIcon smartTypeTrayIcon() {
    const bool darkTheme = QApplication::palette().color(QPalette::Window).lightness() < 128;
    return iconFromResource(darkTheme
                                ? QStringLiteral(":/smarttype/icons/tray-dark.png")
                                : QStringLiteral(":/smarttype/icons/tray-light.png"));
}

class Tray final : public QObject {
public:
    explicit Tray(QObject* parent = nullptr)
        : QObject(parent), tray_(this), menu_(), statusTimer_(this), statusProcess_(this) {
        tray_.setIcon(smartTypeTrayIcon());
        tray_.setToolTip(QStringLiteral("SmartType"));
        tray_.setContextMenu(&menu_);
        connect(&menu_, &QMenu::aboutToShow, this, [this]() { rebuild(); });
        qApp->installEventFilter(this);
        connect(&tray_, &QSystemTrayIcon::activated, this,
                [this](QSystemTrayIcon::ActivationReason reason) {
                    if (reason == QSystemTrayIcon::Trigger) {
                        rebuild();
                        menu_.popup(QCursor::pos());
                    }
                });
        rebuild();

        if (!QSystemTrayIcon::isSystemTrayAvailable()) {
            showSettings(0);
            if (activeSettingsDialog_) {
                connect(activeSettingsDialog_, &QDialog::finished, qApp, &QCoreApplication::quit);
            } else {
                QTimer::singleShot(0, qApp, &QCoreApplication::quit);
            }
        } else {
            tray_.show();
        }

        connect(&statusTimer_, &QTimer::timeout, this, [this]() { refreshRuntimeStatus(); });
        connect(&statusProcess_, &QProcess::finished, this,
                [this](int, QProcess::ExitStatus) {
                    currentInputMethod_ = QString::fromUtf8(statusProcess_.readAllStandardOutput()).trimmed();
                    runtimeActive_ = currentInputMethod_.startsWith(QStringLiteral("smarttype"));
                    updateIcon();
                });
        statusTimer_.start(2000);
        refreshRuntimeStatus();
    }

    void showSettings(int tab = 0) {
        if (activeSettingsDialog_) {
            activeSettingsDialog_->raise();
            activeSettingsDialog_->activateWindow();
            return;
        }
        activeSettingsDialog_ = new SettingsDialog(nullptr, tab);
        activeSettingsDialog_->setAttribute(Qt::WA_DeleteOnClose);
        connect(activeSettingsDialog_, &QObject::destroyed, this, [this]() {
            activeSettingsDialog_ = nullptr;
        });
        activeSettingsDialog_->show();
    }

private:
    bool eventFilter(QObject* watched, QEvent* event) override {
        if (watched == qApp && event->type() == QEvent::ApplicationPaletteChange) {
            updateIcon();
        }
        return QObject::eventFilter(watched, event);
    }

    void refreshRuntimeStatus() {
        if (statusProcess_.state() != QProcess::NotRunning) return;
        statusProcess_.start(QStringLiteral("fcitx5-remote"), {QStringLiteral("-n")});
    }

    void updateIcon() {
        tray_.setIcon(smartTypeTrayIcon());
        tray_.setToolTip(!runtimeActive_
                             ? QStringLiteral("SmartType не выбран в Fcitx")
                             : currentInputMethod_ == QStringLiteral("smarttype-us")
                                   ? QStringLiteral("SmartType — English")
                                   : QStringLiteral("SmartType — Русский"));
    }

    void rebuild() {
        menu_.clear();
        const bool enabled = store_.setting_enabled("enabled");
        const auto currentApp = store_.string_setting("current_app");
        const bool paused = !currentApp.empty() && store_.is_app_blacklisted(currentApp);
        updateIcon();
        const QString status = !enabled ? QStringLiteral("SmartType выключен")
            : !runtimeActive_ ? QStringLiteral("SmartType не выбран в Fcitx")
            : currentInputMethod_ == QStringLiteral("smarttype-us")
                ? QStringLiteral("SmartType включён — English")
                : QStringLiteral("SmartType включён — Русский");
        auto* heading = menu_.addAction(status);
        heading->setEnabled(false);

        auto* toggle = menu_.addAction(QStringLiteral("Включить SmartType"));
        toggle->setCheckable(true);
        toggle->setChecked(enabled);
        connect(toggle, &QAction::toggled, this, [this](bool checked) {
            store_.set_setting("enabled", checked);
        });

        auto* pauseApp = menu_.addAction(currentApp.empty()
            ? QStringLiteral("Приостановить в текущем приложении")
            : paused ? QStringLiteral("Возобновить в %1").arg(QString::fromStdString(currentApp))
                     : QStringLiteral("Приостановить в %1").arg(QString::fromStdString(currentApp)));
        pauseApp->setEnabled(!currentApp.empty());
        connect(pauseApp, &QAction::triggered, this, [this, currentApp, paused]() {
            if (!currentApp.empty()) {
                if (paused) store_.blacklist_remove(currentApp);
                else store_.blacklist_add(currentApp);
            }
        });

        auto* settings = menu_.addAction(QStringLiteral("Открыть настройки…"));
        connect(settings, &QAction::triggered, this, [this]() {
            showSettings(0);
        });
        auto* dictionary = menu_.addAction(QStringLiteral("Открыть личный словарь…"));
        connect(dictionary, &QAction::triggered, this, [this]() {
            showSettings(2);
        });

        menu_.addSeparator();
        auto* historyMenu = menu_.addMenu(QStringLiteral("Последние исправления"));
        const auto entries = store_.history(8);
        if (entries.empty()) {
            auto* empty = historyMenu->addAction(QStringLiteral("Пока исправлений нет"));
            empty->setEnabled(false);
        }

        auto* undoLast = menu_.addAction(QStringLiteral("Отменить последнее исправление"));
        undoLast->setEnabled(!entries.empty() && !entries.front().undone);
        connect(undoLast, &QAction::triggered, this, [this]() {
            const auto request = std::chrono::steady_clock::now().time_since_epoch().count();
            store_.set_string_setting("undo_request_id", std::to_string(request));
        });
        for (const auto& entry : entries) {
            const auto prefix = entry.undone ? QStringLiteral("↶ ") : QString();
            auto* item = historyMenu->addMenu(
                prefix + QString::fromStdString(entry.original) + QStringLiteral(" → ") +
                QString::fromStdString(entry.replacement));
            if (!entry.app.empty()) {
                auto* app = item->addAction(QStringLiteral("Приложение: %1")
                                                .arg(QString::fromStdString(entry.app)));
                app->setEnabled(false);
            }
            auto* disable = item->addAction(QStringLiteral("Больше не исправлять автоматически"));
            connect(disable, &QAction::triggered, this, [this, entry]() {
                store_.disable_rule(entry.original, entry.replacement);
            });
        }

        menu_.addSeparator();
        auto* report = menu_.addAction(QStringLiteral("Создать отчёт о проблеме…"));
        connect(report, &QAction::triggered, this, []() {
            IssueReportDialog dialog;
            dialog.exec();
        });
        auto* project = menu_.addAction(QStringLiteral("Проект и поддержка на GitHub…"));
        connect(project, &QAction::triggered, this, []() {
            QDesktopServices::openUrl(QUrl(QString::fromLatin1(kProjectUrl)));
        });

        // The tray is the only always-available entry point to SmartType.
        // Do not offer a "close icon" action: it used to stop the tray without
        // explaining how to open settings again until the next login.
    }

    smarttype::PersonalStore store_;
    QSystemTrayIcon tray_;
    QMenu menu_;
    QTimer statusTimer_;
    QProcess statusProcess_;
    QString currentInputMethod_;
    bool runtimeActive_{true};
    SettingsDialog* activeSettingsDialog_{nullptr};
};

}  // namespace

int main(int argc, char** argv) {
    std::cerr << "[SmartType Tray Startup] PID: " << getpid() << "\n"
              << "  Executable  : " << (argc > 0 ? argv[0] : "unknown") << "\n"
              << "  Version Hash: " << smarttype::GIT_COMMIT_HASH << "\n"
              << "  Build Time  : " << smarttype::BUILD_TIMESTAMP << std::endl;

    QApplication application(argc, argv);
    QApplication::setApplicationName(QStringLiteral("SmartType"));
    QApplication::setDesktopFileName(QStringLiteral("smarttype-tray"));
    QApplication::setWindowIcon(iconFromResource(
        QStringLiteral(":/smarttype/icons/application.png")));
    QApplication::setQuitOnLastWindowClosed(false);
    const auto arguments = QApplication::arguments();
    if (arguments.contains(QStringLiteral("--check-settings"))) {
        SettingsDialog dialog;
        return 0;
    }
    if (arguments.contains(QStringLiteral("--check-report"))) {
        IssueReportDialog dialog;
        return 0;
    }
    if (arguments.contains(QStringLiteral("--check-icons"))) {
        const QPixmap dark(QStringLiteral(":/smarttype/icons/tray-dark.png"));
        const QPixmap light(QStringLiteral(":/smarttype/icons/tray-light.png"));
        const QPixmap applicationIcon(QStringLiteral(":/smarttype/icons/application.png"));
        const bool ok = !dark.isNull() && !light.isNull() && !applicationIcon.isNull() &&
                        !smartTypeTrayIcon().isNull() && !QApplication::windowIcon().isNull();
        if (!ok) {
            std::cerr << "Tray icon resource check failed: dark=" << !dark.isNull()
                      << " light=" << !light.isNull()
                      << " application=" << !applicationIcon.isNull()
                      << " tray=" << !smartTypeTrayIcon().isNull()
                      << " window=" << !QApplication::windowIcon().isNull() << '\n';
        }
        return ok ? 0 : 1;
    }
    if (arguments.contains(QStringLiteral("--settings")) ||
        arguments.contains(QStringLiteral("--dictionary"))) {
        SettingsDialog dialog(nullptr, arguments.contains(QStringLiteral("--dictionary")) ? 2 : 0);
        dialog.show();
        return application.exec();
    }

    const QString socketName = QStringLiteral("smarttype-tray-socket");
    {
        QLocalSocket socket;
        socket.connectToServer(socketName);
        if (socket.waitForConnected(200)) {
            socket.write("show-settings");
            socket.waitForBytesWritten(200);
            return 0;
        }
    }

    QLocalServer::removeServer(socketName);
    auto* server = new QLocalServer(&application);
    if (!server->listen(socketName)) {
        // failed to listen, but proceed
    }

    Tray tray;

    QObject::connect(server, &QLocalServer::newConnection, &tray, [&tray, server]() {
        auto* socket = server->nextPendingConnection();
        if (socket) {
            QObject::connect(socket, &QLocalSocket::readyRead, &tray, [&tray, socket]() {
                const auto data = socket->readAll();
                if (data == "show-settings") {
                    tray.showSettings(0);
                }
            });
            QObject::connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
        }
    });

    return application.exec();
}
