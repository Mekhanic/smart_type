#include <QApplication>
#include <QStatusBar>
#include "themelabwindow.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("SmartType Theme Lab"));
    app.setApplicationVersion(QStringLiteral("0.1"));
    app.setOrganizationName(QStringLiteral("SmartType"));

    smarttype::themelab::ThemeLabWindow window;

    // If a path is given on the command line, show a hint in status bar.
    // The user can then click Load to open the file through the dialog.
    const QStringList args = app.arguments();
    if (args.size() >= 2) {
        window.statusBar()->showMessage(
            QStringLiteral("Tip: use Load button to open: ") + args[1]);
    }

    window.show();
    return app.exec();
}

