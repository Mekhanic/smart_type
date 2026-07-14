#pragma once

#include "smarttype/personal_store.hpp"

#include <QDialog>

class QListWidget;
class QLabel;
class QSlider;

class SettingsDialog final : public QDialog {
public:
    explicit SettingsDialog(QWidget* parent = nullptr, int initial_tab = 0);

private:
    void refreshApplications();
    void refreshDictionary();
    void refreshBlocked();
    void refreshHistory();
    void refreshDiagnostics();
    void exportData();
    void importData();

    // Fcitx DBus helpers for CandidatePanelScale
    // Returns 80–130 (fallback 100 on missing/corrupt key), or -1 if Fcitx unavailable.
    int loadPanelScale();
    // SetConfig + re-GetConfig verification. Returns true if the saved value matches.
    bool applyPanelScale(int value);

    void setPanelScaleUi(int value, bool enabled);
    void onPanelScaleSliderMoved(int value);
    void onApply();
    void onOk();

    smarttype::PersonalStore store_;
    QListWidget* applications_{nullptr};
    QListWidget* dictionary_{nullptr};
    QListWidget* blocked_{nullptr};
    QListWidget* history_{nullptr};
    QListWidget* diagnostics_{nullptr};
    QLabel*      statsLabel_{nullptr};
    QLabel*      applicationsEmptyLabel_{nullptr};
    QLabel*      blockedEmptyLabel_{nullptr};
    QLabel*      dictionaryEmptyLabel_{nullptr};
    QSlider*     panelScale_{nullptr};
    QLabel*      panelScaleValue_{nullptr};
    QLabel*      panelScaleStatus_{nullptr};
    bool         panelScaleAvailable_{false};
    int          panelScaleLoaded_{100};
};
