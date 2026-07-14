#pragma once

#include "smarttype/personal_store.hpp"

#include <QDialog>

class QComboBox;
class QTextEdit;

class IssueReportDialog final : public QDialog {
public:
    explicit IssueReportDialog(QWidget* parent = nullptr);

private:
    void createReport();
    void loadRecentEvents();
    QString runCommand(const QString& program, const QStringList& arguments) const;
    QString readFile(const QString& path) const;
    QString recentLayoutEvents(int seconds) const;
    QString recentCorrections(int seconds) const;
    QString relevantJournal(int seconds) const;

    smarttype::PersonalStore store_;
    QComboBox* interval_{nullptr};
    QComboBox* event_{nullptr};
    QTextEdit* description_{nullptr};
};
