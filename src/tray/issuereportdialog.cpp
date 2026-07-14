#include "issuereportdialog.hpp"

#include <QComboBox>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QStandardPaths>
#include <QTextEdit>
#include <QVBoxLayout>

namespace {

QString fenced(const QString& value) {
    return QStringLiteral("```text\n%1\n```\n").arg(value.trimmed());
}

QString layoutLogPath() {
    const auto state = qEnvironmentVariable("XDG_STATE_HOME",
                                             QDir::homePath() + QStringLiteral("/.local/state"));
    return state + QStringLiteral("/smarttype/layout-events.jsonl");
}

}  // namespace

IssueReportDialog::IssueReportDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle(QStringLiteral("Отчёт о проблеме SmartType"));
    resize(600, 480);
    auto* layout = new QVBoxLayout(this);

    layout->addWidget(new QLabel(QStringLiteral("Что происходило перед проблемой?"), this));
    interval_ = new QComboBox(this);
    interval_->addItem(QStringLiteral("Последняя 1 минута"), 60);
    interval_->addItem(QStringLiteral("Последние 5 минут"), 300);
    interval_->addItem(QStringLiteral("Последние 15 минут"), 900);
    interval_->addItem(QStringLiteral("Последние 30 минут"), 1800);
    interval_->addItem(QStringLiteral("Последний час"), 3600);
    interval_->setCurrentIndex(1);
    layout->addWidget(interval_);

    layout->addWidget(new QLabel(QStringLiteral("Конкретное событие (необязательно):"), this));
    event_ = new QComboBox(this);
    event_->addItem(QStringLiteral("Без конкретного события"), QString());
    loadRecentEvents();
    layout->addWidget(event_);

    layout->addWidget(new QLabel(QStringLiteral("Опишите, что ожидалось и что произошло:"), this));
    description_ = new QTextEdit(this);
    description_->setPlaceholderText(QStringLiteral(
        "Например: выбрал English мышкой, но в Telegram продолжали вводиться русские буквы."));
    layout->addWidget(description_);

    auto* privacy = new QLabel(QStringLiteral(
        "В отчёт не копируются сообщения, пароль, буфер обмена или полный набранный текст. "
        "Попадут системные состояния, события раскладки и выбранное вами исправление."), this);
    privacy->setWordWrap(true);
    layout->addWidget(privacy);

    auto* create = new QPushButton(QStringLiteral("Сохранить отчёт…"), this);
    connect(create, &QPushButton::clicked, this, [this]() { createReport(); });
    layout->addWidget(create, 0, Qt::AlignRight);
}

QString IssueReportDialog::runCommand(const QString& program, const QStringList& arguments) const {
    QProcess process;
    process.start(program, arguments);
    if (!process.waitForFinished(4000)) {
        process.kill();
        return QStringLiteral("Команда не ответила: %1").arg(program);
    }
    const auto output = QString::fromUtf8(process.readAllStandardOutput());
    const auto error = QString::fromUtf8(process.readAllStandardError());
    return (output + (error.isEmpty() ? QString() : QStringLiteral("\n[stderr]\n") + error)).trimmed();
}

QString IssueReportDialog::readFile(const QString& path) const {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return QStringLiteral("Недоступно: %1").arg(path);
    return QString::fromUtf8(file.readAll());
}

void IssueReportDialog::loadRecentEvents() {
    const auto history = store_.history(25);
    for (const auto& entry : history) {
        const auto time = QDateTime::fromSecsSinceEpoch(entry.created_at).toString(QStringLiteral("dd.MM hh:mm:ss"));
        const auto title = QStringLiteral("%1 · исправление: %2 → %3%4")
            .arg(time, QString::fromStdString(entry.original), QString::fromStdString(entry.replacement),
                 entry.undone ? QStringLiteral(" (отменено)") : QString());
        const auto details = QStringLiteral("correction id=%1, time=%2, app=%3, source=%4, undone=%5, %6 -> %7")
            .arg(entry.id).arg(entry.created_at).arg(QString::fromStdString(entry.app),
                 QString::fromStdString(entry.source), entry.undone ? QStringLiteral("yes") : QStringLiteral("no"),
                 QString::fromStdString(entry.original), QString::fromStdString(entry.replacement));
        event_->addItem(title, details);
    }

    QFile file(layoutLogPath());
    if (!file.open(QIODevice::ReadOnly)) return;
    const auto lines = file.readAll().split('\n');
    const int start = qMax(0, lines.size() - 25);
    for (int index = lines.size() - 1; index >= start; --index) {
        const auto object = QJsonDocument::fromJson(lines[index]).object();
        if (object.isEmpty()) continue;
        const auto title = QStringLiteral("%1 · раскладка: %2 → %3%4")
            .arg(QDateTime::fromString(object.value(QStringLiteral("time")).toString(), Qt::ISODate)
                     .toLocalTime().toString(QStringLiteral("dd.MM hh:mm:ss")),
                 object.value(QStringLiteral("before")).toString(),
                 object.value(QStringLiteral("after")).toString(),
                 object.value(QStringLiteral("ok")).toBool() ? QString() : QStringLiteral(" (ошибка)"));
        event_->addItem(title, QStringLiteral("layout-event: ") + QString::fromUtf8(lines[index]));
    }
}

QString IssueReportDialog::recentLayoutEvents(int seconds) const {
    QFile file(layoutLogPath());
    if (!file.open(QIODevice::ReadOnly)) return QStringLiteral("Журнал раскладки пока пуст.");
    const auto cutoff = QDateTime::currentDateTime().addSecs(-seconds);
    QStringList selected;
    for (const auto& line : file.readAll().split('\n')) {
        const auto object = QJsonDocument::fromJson(line).object();
        const auto time = QDateTime::fromString(object.value(QStringLiteral("time")).toString(), Qt::ISODate);
        if (!object.isEmpty() && time.isValid() && time >= cutoff) selected.append(QString::fromUtf8(line));
    }
    return selected.isEmpty() ? QStringLiteral("Событий за этот интервал нет.") : selected.join('\n');
}

QString IssueReportDialog::recentCorrections(int seconds) const {
    const auto cutoff = QDateTime::currentSecsSinceEpoch() - seconds;
    QStringList result;
    for (const auto& entry : store_.history(100)) {
        if (entry.created_at < cutoff) continue;
        result.append(QStringLiteral("time=%1 app=%2 source=%3 undone=%4  %5 -> %6")
            .arg(QDateTime::fromSecsSinceEpoch(entry.created_at).toString(Qt::ISODate),
                 QString::fromStdString(entry.app), QString::fromStdString(entry.source),
                 entry.undone ? QStringLiteral("yes") : QStringLiteral("no"),
                 QString::fromStdString(entry.original), QString::fromStdString(entry.replacement)));
    }
    return result.isEmpty() ? QStringLiteral("Исправлений за этот интервал нет.") : result.join('\n');
}

QString IssueReportDialog::relevantJournal(int seconds) const {
    const auto raw = runCommand(QStringLiteral("journalctl"),
        {QStringLiteral("--user"), QStringLiteral("--since=-%1 seconds").arg(seconds),
         QStringLiteral("--no-pager"), QStringLiteral("-o"), QStringLiteral("short-iso")});
    QStringList result;
    for (const auto& line : raw.split('\n')) {
        if (line.contains(QStringLiteral("fcitx"), Qt::CaseInsensitive) ||
            line.contains(QStringLiteral("smarttype"), Qt::CaseInsensitive) ||
            line.contains(QStringLiteral("keyboard"), Qt::CaseInsensitive) ||
            line.contains(QStringLiteral("layout"), Qt::CaseInsensitive)) {
            result.append(line);
        }
    }
    return result.isEmpty() ? QStringLiteral("Подходящих строк журнала нет.") : result.join('\n');
}

void IssueReportDialog::createReport() {
    const int seconds = interval_->currentData().toInt();
    const auto timestamp = QDateTime::currentDateTime();
    const auto documents = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    const auto suggested = QDir(documents).filePath(
        QStringLiteral("smarttype-report-%1.md").arg(timestamp.toString(QStringLiteral("yyyyMMdd-HHmmss"))));
    const auto path = QFileDialog::getSaveFileName(this, QStringLiteral("Сохранить отчёт"), suggested,
                                                   QStringLiteral("Markdown (*.md)"));
    if (path.isEmpty()) return;

    QString report;
    report += QStringLiteral("# Отчёт о проблеме SmartType\n\n");
    report += QStringLiteral("Создан: %1\n\nИнтервал: последние %2 секунд\n\n")
                  .arg(timestamp.toString(Qt::ISODate)).arg(seconds);
    report += QStringLiteral("## Описание пользователя\n\n%1\n\n")
                  .arg(description_->toPlainText().trimmed().isEmpty()
                           ? QStringLiteral("Не указано.") : description_->toPlainText().trimmed());
    report += QStringLiteral("## Выбранное событие\n\n") +
              fenced(event_->currentData().toString().isEmpty()
                         ? QStringLiteral("Не выбрано.") : event_->currentData().toString()) + QStringLiteral("\n");
    report += QStringLiteral("## Текущее состояние\n\n### Fcitx input method\n") +
              fenced(runCommand(QStringLiteral("fcitx5-remote"), {QStringLiteral("-n")}));
    report += QStringLiteral("\n### KDE layout\n") + fenced(runCommand(
        QStringLiteral("busctl"), {QStringLiteral("--user"), QStringLiteral("call"),
        QStringLiteral("org.kde.keyboard"), QStringLiteral("/Layouts"),
        QStringLiteral("org.kde.KeyboardLayouts"), QStringLiteral("getLayout")}));
    report += QStringLiteral("\n### Processes\n") + fenced(runCommand(
        QStringLiteral("pgrep"), {QStringLiteral("-a"), QStringLiteral("-f"),
        QStringLiteral("fcitx5|smarttype|layout-sync")}));
    report += QStringLiteral("\n### Layout synchronizer\n") + fenced(runCommand(
        QStringLiteral("systemctl"), {QStringLiteral("--user"), QStringLiteral("status"),
        QStringLiteral("fcitx5-layout-sync.service"), QStringLiteral("--no-pager")}));
    report += QStringLiteral("\n## События раскладки\n") + fenced(recentLayoutEvents(seconds));
    report += QStringLiteral("\n## Исправления за интервал\n") + fenced(recentCorrections(seconds));
    report += QStringLiteral("\n## Журнал служб\n") + fenced(relevantJournal(seconds));
    report += QStringLiteral("\n## Конфигурация\n\n### kxkbrc\n") +
              fenced(readFile(QDir::homePath() + QStringLiteral("/.config/kxkbrc")));
    report += QStringLiteral("\n### Fcitx profile\n") +
              fenced(readFile(QDir::homePath() + QStringLiteral("/.config/fcitx5/profile")));
    report += QStringLiteral("\n### Fcitx config\n") +
              fenced(readFile(QDir::homePath() + QStringLiteral("/.config/fcitx5/config")));
    report += QStringLiteral("\n## Конфиденциальность\n\nПолный набранный текст, сообщения, пароли и буфер обмена не собирались.\n");

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate) || file.write(report.toUtf8()) < 0) {
        QMessageBox::critical(this, QStringLiteral("Отчёт"), QStringLiteral("Не удалось сохранить файл."));
        return;
    }
    file.close();
    QMessageBox::information(this, QStringLiteral("Отчёт готов"),
                             QStringLiteral("Сохранено:\n%1\n\nЭтот файл можно передать для диагностики.").arg(path));
    accept();
}
