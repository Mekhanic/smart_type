#include "settingsdialog.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusVariant>
#include <QDialogButtonBox>
#include <QDesktopServices>
#include <QFile>
#include <QFileDialog>
#include <QFont>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSlider>
#include <QTabWidget>
#include <QUrl>
#include <QVBoxLayout>
#include <QVariantMap>

#include <functional>
#include <initializer_list>
#include <string_view>

namespace {

constexpr int kPanelScaleMin = 80;
constexpr int kPanelScaleMax = 130;
constexpr int kPanelScaleDefault = 100;
constexpr int kPanelScaleStep = 5;
constexpr auto kProjectUrl = "https://github.com/Mekhanic/smart_type";

int snapPanelScale(int value) {
    const int snapped =
        qRound(static_cast<double>(value) / static_cast<double>(kPanelScaleStep)) * kPanelScaleStep;
    return qBound(kPanelScaleMin, snapped, kPanelScaleMax);
}
constexpr auto kSmarttypeUiConfigUri = "fcitx://config/addon/smarttypeui";
constexpr auto kCandidatePanelScaleKey = "CandidatePanelScale";
constexpr auto kFcitxService = "org.fcitx.Fcitx5";
constexpr auto kFcitxPath = "/controller";
constexpr auto kFcitxIface = "org.fcitx.Fcitx.Controller1";

QWidget* listPage(QListWidget*& list, const QString& addTitle, const QString& removeTitle,
                  std::function<void()> add, std::function<void()> remove) {
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);
    list = new QListWidget(page);
    layout->addWidget(list);
    auto* buttons = new QHBoxLayout;
    auto* addButton = new QPushButton(addTitle, page);
    auto* removeButton = new QPushButton(removeTitle, page);
    buttons->addWidget(addButton);
    buttons->addWidget(removeButton);
    buttons->addStretch();
    layout->addLayout(buttons);
    addButton->setVisible(!addTitle.isEmpty());
    QObject::connect(addButton, &QPushButton::clicked, page, std::move(add));
    QObject::connect(removeButton, &QPushButton::clicked, page, std::move(remove));
    return page;
}

// Fcitx GetConfig returns variant(a{sv}) which Qt delivers as QDBusVariant → QDBusArgument,
// not a ready-made QVariantMap. Nested values may themselves be QDBusVariant-wrapped strings.
QVariant unwrapDbus(QVariant value) {
    for (int i = 0; i < 4; ++i) {
        if (value.userType() == qMetaTypeId<QDBusVariant>()) {
            const QVariant inner = qvariant_cast<QDBusVariant>(value).variant();
            if (!inner.isValid()) break;
            value = inner;
            continue;
        }
        break;
    }
    return value;
}

QVariantMap dbusVariantToMap(const QVariant& value) {
    const QVariant unwrapped = unwrapDbus(value);
    if (unwrapped.userType() == QMetaType::QVariantMap) {
        return unwrapped.toMap();
    }
    if (unwrapped.userType() == qMetaTypeId<QDBusArgument>()) {
        QDBusArgument arg = qvariant_cast<QDBusArgument>(unwrapped);
        // Signature is typically a{sv}.
        QVariantMap map;
        arg >> map;
        return map;
    }
    if (unwrapped.canConvert<QVariantMap>()) {
        return unwrapped.toMap();
    }
    return {};
}

// Parse CandidatePanelScale from a config map entry. Missing/corrupt → fallback 100.
int parsePanelScaleValue(const QVariant& rawIn) {
    const QVariant raw = unwrapDbus(rawIn);
    if (!raw.isValid() || raw.isNull()) {
        return kPanelScaleDefault;
    }

    bool ok = false;
    int value = 0;
    if (raw.userType() == QMetaType::QString || raw.canConvert<QString>()) {
        value = raw.toString().trimmed().toInt(&ok);
    } else if (raw.canConvert<int>()) {
        value = raw.toInt(&ok);
        if (!ok) {
            ok = true;
            value = raw.toInt();
        }
    } else {
        return kPanelScaleDefault;
    }

    if (!ok || value < kPanelScaleMin || value > kPanelScaleMax) {
        return kPanelScaleDefault;
    }
    return value;
}

bool fetchSmarttypeUiConfig(QVariantMap* outMap, QString* errorOut) {
    if (!QDBusConnection::sessionBus().isConnected()) {
        if (errorOut) *errorOut = QStringLiteral("Fcitx 5 не запущен");
        return false;
    }

    QDBusMessage call = QDBusMessage::createMethodCall(
        QString::fromLatin1(kFcitxService), QString::fromLatin1(kFcitxPath),
        QString::fromLatin1(kFcitxIface), QStringLiteral("GetConfig"));
    call << QString::fromLatin1(kSmarttypeUiConfigUri);

    const QDBusMessage reply = QDBusConnection::sessionBus().call(call);
    if (reply.type() == QDBusMessage::ErrorMessage) {
        if (errorOut) {
            const QString name = reply.errorName();
            if (name.contains(QStringLiteral("ServiceUnknown"), Qt::CaseInsensitive) ||
                name.contains(QStringLiteral("NameHasNoOwner"), Qt::CaseInsensitive)) {
                *errorOut = QStringLiteral("Fcitx 5 не запущен");
            } else {
                *errorOut = QStringLiteral("Fcitx 5 не запущен");
            }
        }
        return false;
    }
    if (reply.arguments().isEmpty()) {
        if (errorOut) *errorOut = QStringLiteral("Fcitx 5 не запущен");
        return false;
    }

    const QVariantMap map = dbusVariantToMap(reply.arguments().at(0));
    if (map.isEmpty() && errorOut) {
        // Empty map may still be valid for a fresh addon; treat as success with no keys.
    }
    if (outMap) *outMap = map;
    return true;
}

bool pushSmarttypeUiConfig(const QVariantMap& map, QString* errorOut) {
    if (!QDBusConnection::sessionBus().isConnected()) {
        if (errorOut) *errorOut = QStringLiteral("Fcitx 5 не запущен");
        return false;
    }

    // Fcitx stores option values as strings in the D-Bus a{sv} payload.
    // Partial maps are valid (merge); unwrap nested QDBusVariant values first.
    QVariantMap stringMap;
    for (auto it = map.constBegin(); it != map.constEnd(); ++it) {
        stringMap.insert(it.key(), unwrapDbus(it.value()).toString());
    }

    QDBusMessage call = QDBusMessage::createMethodCall(
        QString::fromLatin1(kFcitxService), QString::fromLatin1(kFcitxPath),
        QString::fromLatin1(kFcitxIface), QStringLiteral("SetConfig"));
    // Outer QDBusVariant matches SetConfig's `v` argument; map marshals as a{sv}.
    call << QString::fromLatin1(kSmarttypeUiConfigUri)
         << QVariant::fromValue(QDBusVariant(QVariant::fromValue(stringMap)));

    const QDBusMessage reply = QDBusConnection::sessionBus().call(call);
    if (reply.type() == QDBusMessage::ErrorMessage) {
        if (errorOut) {
            // Prefer a clear offline message; include D-Bus detail for diagnostics builds.
            *errorOut = QStringLiteral("Fcitx 5 не запущен");
        }
        return false;
    }
    return true;
}

}  // namespace

SettingsDialog::SettingsDialog(QWidget* parent, int initial_tab) : QDialog(parent) {
    setWindowTitle(QStringLiteral("Настройки SmartType"));
    resize(620, 520);
    auto* root = new QVBoxLayout(this);
    auto* tabs = new QTabWidget(this);
    root->addWidget(tabs);

    auto* generalScroll = new QScrollArea(tabs);
    generalScroll->setWidgetResizable(true);
    generalScroll->setFrameShape(QFrame::NoFrame);
    auto* general = new QWidget(generalScroll);
    auto* generalLayout = new QVBoxLayout(general);
    auto addToggleGroup = [this, general](QVBoxLayout* parent,
                                          const QString& title,
                                          const QString& description,
                                          std::initializer_list<std::pair<const char*, const char*>> toggles) {
        auto* group = new QGroupBox(title, general);
        auto* layout = new QVBoxLayout(group);
        if (!description.isEmpty()) {
            auto* hint = new QLabel(description, group);
            hint->setWordWrap(true);
            hint->setStyleSheet(QStringLiteral("color: palette(mid);"));
            layout->addWidget(hint);
        }
        for (const auto& [key, label] : toggles) {
            auto* check = new QCheckBox(QString::fromUtf8(label), group);
            check->setChecked(store_.setting_enabled(key, std::string_view(key) != "diagnostics"));
            connect(check, &QCheckBox::toggled, this,
                    [this, key](bool enabled) { store_.set_setting(key, enabled); });
            layout->addWidget(check);
        }
        parent->addWidget(group);
    };
    addToggleGroup(generalLayout, QStringLiteral("Работа SmartType"),
                   QStringLiteral("Эти параметры определяют, исправляет ли SmartType текст и показывает ли варианты."),
                   {{"enabled", "Включить SmartType"},
                    {"autocorrect", "Исправлять автоматически при высокой уверенности"},
                    {"suggestions", "Показывать варианты исправления"},
                    {"learning", "Запоминать мои исправления"}});
    addToggleGroup(generalLayout, QStringLiteral("Набор текста"),
                   QStringLiteral("Не меняет словарь — только оформление ввода."),
                   {{"sentence_capitalization", "Начинать предложение с заглавной буквы"},
                    {"smart_punctuation", "Умная пунктуация: —, « »"},
                    {"auto_space_after_punctuation", "Ставить пробел после знака пунктуации"},
                    {"accidental_case", "Исправлять случайный регистр"},
                    {"inline_correction_flash", "Кратко подсвечивать автоисправление"}});

    auto* layoutContainer = new QGroupBox(QStringLiteral("Раскладка и внешний вид"), general);
    auto* layoutGroupLayout = new QVBoxLayout(layoutContainer);
    auto* layoutRow = new QWidget(layoutContainer);
    auto* layoutHLayout = new QHBoxLayout(layoutRow);
    layoutHLayout->setContentsMargins(0, 0, 0, 0);
    auto* layoutLabel = new QLabel(QStringLiteral("Исправление неправильной раскладки:"), layoutContainer);
    auto* layoutCombo = new QComboBox(layoutContainer);
    layoutCombo->addItem(QStringLiteral("Выключено"), QStringLiteral("disabled"));
    layoutCombo->addItem(QStringLiteral("Только предлагать"), QStringLiteral("suggest"));
    layoutCombo->addItem(QStringLiteral("Автоматически"), QStringLiteral("auto"));

    const bool layout_corr = store_.setting_enabled("layout_correction", true);
    const std::string layout_mode = store_.string_setting("layout_mode", "suggest");
    if (!layout_corr || layout_mode == "disabled") {
        layoutCombo->setCurrentIndex(0);
    } else if (layout_mode == "auto") {
        layoutCombo->setCurrentIndex(2);
    } else {
        layoutCombo->setCurrentIndex(1);
    }

    connect(layoutCombo, &QComboBox::currentIndexChanged, this, [this, layoutCombo](int index) {
        const QString data = layoutCombo->itemData(index).toString();
        if (data == QStringLiteral("disabled")) {
            store_.set_setting("layout_correction", false);
            store_.set_string_setting("layout_mode", "disabled");
        } else if (data == QStringLiteral("suggest")) {
            store_.set_setting("layout_correction", true);
            store_.set_string_setting("layout_mode", "suggest");
        } else if (data == QStringLiteral("auto")) {
            store_.set_setting("layout_correction", true);
            store_.set_string_setting("layout_mode", "auto");
        }
    });

    layoutHLayout->addWidget(layoutLabel);
    layoutHLayout->addWidget(layoutCombo);
    layoutHLayout->addStretch();
    layoutGroupLayout->addWidget(layoutRow);

    // Candidate panel scale (smarttypeui via Fcitx D-Bus — not PersonalStore).
    // Slider only updates the on-screen label; Apply/OK persist via SetConfig.
    auto* scaleBlock = new QWidget(layoutContainer);
    auto* scaleBlockLayout = new QVBoxLayout(scaleBlock);
    scaleBlockLayout->setContentsMargins(0, 0, 0, 0);
    scaleBlockLayout->setSpacing(4);
    auto* scaleTitle = new QLabel(QStringLiteral("Размер панели кандидатов"), scaleBlock);
    scaleBlockLayout->addWidget(scaleTitle);

    auto* scaleRow = new QWidget(scaleBlock);
    auto* scaleLayout = new QHBoxLayout(scaleRow);
    scaleLayout->setContentsMargins(0, 0, 0, 0);
    panelScale_ = new QSlider(Qt::Horizontal, scaleRow);
    panelScale_->setMinimum(kPanelScaleMin);
    panelScale_->setMaximum(kPanelScaleMax);
    panelScale_->setSingleStep(kPanelScaleStep);
    panelScale_->setPageStep(kPanelScaleStep);
    panelScale_->setTickInterval(kPanelScaleStep);
    panelScale_->setTickPosition(QSlider::TicksBelow);
    panelScale_->setValue(kPanelScaleDefault);
    panelScaleValue_ = new QLabel(scaleRow);
    panelScaleValue_->setMinimumWidth(48);
    panelScaleValue_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    panelScaleValue_->setText(QStringLiteral("%1%").arg(kPanelScaleDefault));
    scaleLayout->addWidget(panelScale_, /*stretch=*/1);
    scaleLayout->addWidget(panelScaleValue_);
    scaleBlockLayout->addWidget(scaleRow);
    auto* scaleHint = new QLabel(
        QStringLiteral("Нажмите «Применить» или «OK», чтобы сохранить новый размер."),
        scaleBlock);
    scaleHint->setStyleSheet(QStringLiteral("color: palette(mid);"));
    scaleBlockLayout->addWidget(scaleHint);
    layoutGroupLayout->addWidget(scaleBlock);

    connect(panelScale_, &QSlider::valueChanged, this, &SettingsDialog::onPanelScaleSliderMoved);

    panelScaleStatus_ = new QLabel(layoutContainer);
    panelScaleStatus_->setWordWrap(true);
    panelScaleStatus_->setStyleSheet(QStringLiteral("color: #a33;"));
    panelScaleStatus_->hide();
    layoutGroupLayout->addWidget(panelScaleStatus_);

    {
        const int loaded = loadPanelScale();
        if (loaded < 0) {
            panelScaleAvailable_ = false;
            panelScaleLoaded_ = kPanelScaleDefault;
            setPanelScaleUi(kPanelScaleDefault, /*enabled=*/false);
            panelScaleStatus_->setText(QStringLiteral("Fcitx 5 не запущен"));
            panelScaleStatus_->show();
        } else {
            panelScaleAvailable_ = true;
            panelScaleLoaded_ = snapPanelScale(loaded);
            setPanelScaleUi(panelScaleLoaded_, /*enabled=*/true);
            panelScaleStatus_->hide();
        }
    }

    generalLayout->addWidget(layoutContainer);

    addToggleGroup(generalLayout, QStringLiteral("Безопасность и диагностика"),
                   QStringLiteral("Терминалы и редакторы кода можно исключить целиком. Диагностика хранит пары исправлений и причины, не всю переписку."),
                   {{"disable_in_terminals", "Не исправлять в терминалах"},
                    {"disable_in_code", "Не исправлять в редакторах кода"},
                    {"diagnostics", "Сохранять диагностику"}});
    generalLayout->addStretch();
    generalScroll->setWidget(general);
    tabs->addTab(generalScroll, QStringLiteral("Основные"));

    auto* applicationsPage = listPage(
        applications_, QStringLiteral("Добавить"), QStringLiteral("Удалить"),
        [this]() {
            bool ok = false;
            const auto value = QInputDialog::getText(this, QStringLiteral("Исключить приложение"),
                                                     QStringLiteral("Имя процесса или desktop ID:"),
                                                     QLineEdit::Normal, {}, &ok).trimmed();
            if (ok && !value.isEmpty()) store_.blacklist_add(value.toStdString());
            refreshApplications();
        },
        [this]() {
            if (auto* item = applications_->currentItem()) {
                store_.blacklist_remove(item->text().toStdString());
                refreshApplications();
            }
        });
    {
        auto* layout = static_cast<QVBoxLayout*>(applicationsPage->layout());
        auto* hint = new QLabel(
            QStringLiteral("SmartType не будет изменять текст в этих приложениях. Добавляйте desktop ID или часть имени процесса."),
            applicationsPage);
        hint->setWordWrap(true);
        layout->insertWidget(0, hint);
        const auto currentApp = store_.string_setting("current_app");
        auto* addCurrent = new QPushButton(
            currentApp.empty()
                ? QStringLiteral("Сначала поставьте курсор в нужное приложение")
                : QStringLiteral("Не исправлять в текущем: %1").arg(QString::fromStdString(currentApp)),
            applicationsPage);
        addCurrent->setEnabled(!currentApp.empty());
        connect(addCurrent, &QPushButton::clicked, applicationsPage, [this, currentApp]() {
            if (!currentApp.empty()) store_.blacklist_add(currentApp);
            refreshApplications();
        });
        layout->insertWidget(1, addCurrent);
        applicationsEmptyLabel_ = new QLabel(QStringLiteral("Исключений пока нет — SmartType работает во всех поддерживаемых приложениях."), applicationsPage);
        applicationsEmptyLabel_->setWordWrap(true);
        applicationsEmptyLabel_->setStyleSheet(QStringLiteral("color: palette(mid);"));
        layout->insertWidget(3, applicationsEmptyLabel_);
    }
    tabs->addTab(applicationsPage, QStringLiteral("Исключённые приложения"));

    auto* dictionaryPage = listPage(
        dictionary_, QStringLiteral("Добавить слово"), QStringLiteral("Удалить"),
        [this]() {
            bool ok = false;
            const auto value = QInputDialog::getText(this, QStringLiteral("Личный словарь"),
                                                     QStringLiteral("Слово:"), QLineEdit::Normal,
                                                     {}, &ok).trimmed();
            if (ok && !value.isEmpty()) store_.add_word(value.toStdString());
            refreshDictionary();
        },
        [this]() {
            if (auto* item = dictionary_->currentItem()) {
                store_.remove_word(item->text().toStdString());
                refreshDictionary();
            }
        });
    {
        auto* layout = static_cast<QVBoxLayout*>(dictionaryPage->layout());
        auto* hint = new QLabel(QStringLiteral("Добавленные слова SmartType никогда не исправляет автоматически."), dictionaryPage);
        hint->setWordWrap(true);
        layout->insertWidget(0, hint);
        dictionaryEmptyLabel_ = new QLabel(QStringLiteral("Личный словарь пока пуст."), dictionaryPage);
        dictionaryEmptyLabel_->setStyleSheet(QStringLiteral("color: palette(mid);"));
        layout->insertWidget(2, dictionaryEmptyLabel_);
    }
    tabs->addTab(dictionaryPage, QStringLiteral("Личный словарь"));

    auto* blockedPage = listPage(
        blocked_, QString(), QStringLiteral("Разрешить снова"), []() {},
        [this]() {
            if (auto* item = blocked_->currentItem()) {
                const auto typo = item->data(Qt::UserRole).toString();
                const auto correction = item->data(Qt::UserRole + 1).toString();
                store_.enable_rule(typo.toStdString(), correction.toStdString());
                refreshBlocked();
            }
        });
    {
        auto* layout = static_cast<QVBoxLayout*>(blockedPage->layout());
        auto* hint = new QLabel(
            QStringLiteral("Здесь появляются пары «исходный текст → исправление», которые вы запретили через историю. Они больше не будут применяться автоматически."),
            blockedPage);
        hint->setWordWrap(true);
        layout->insertWidget(0, hint);
        blockedEmptyLabel_ = new QLabel(
            QStringLiteral("Запрещённых исправлений пока нет. В истории откройте исправление и выберите «Больше не исправлять автоматически»."),
            blockedPage);
        blockedEmptyLabel_->setWordWrap(true);
        blockedEmptyLabel_->setStyleSheet(QStringLiteral("color: palette(mid);"));
        layout->insertWidget(2, blockedEmptyLabel_);
    }
    tabs->addTab(blockedPage, QStringLiteral("Запрещённые исправления"));

    auto* historyPage = new QWidget(tabs);
    auto* historyLayout = new QVBoxLayout(historyPage);
    statsLabel_ = new QLabel(historyPage);
    statsLabel_->setStyleSheet(QStringLiteral("font-weight: bold; margin-bottom: 5px;"));
    historyLayout->addWidget(statsLabel_);
    history_ = new QListWidget(historyPage);
    historyLayout->addWidget(history_);
    auto* historyButtons = new QHBoxLayout;
    auto* clearHistory = new QPushButton(QStringLiteral("Очистить историю"), historyPage);
    auto* resetLearning = new QPushButton(QStringLiteral("Сбросить обучение"), historyPage);
    auto* exportButton = new QPushButton(QStringLiteral("Экспорт"), historyPage);
    auto* importButton = new QPushButton(QStringLiteral("Импорт"), historyPage);
    historyButtons->addWidget(clearHistory);
    historyButtons->addWidget(resetLearning);
    historyButtons->addStretch();
    historyButtons->addWidget(exportButton);
    historyButtons->addWidget(importButton);
    historyLayout->addLayout(historyButtons);
    connect(clearHistory, &QPushButton::clicked, this, [this]() {
        store_.clear_history();
        refreshHistory();
    });
    connect(resetLearning, &QPushButton::clicked, this, [this]() {
        if (QMessageBox::question(this, QStringLiteral("Сбросить обучение"),
                                  QStringLiteral("Удалить статистику, историю и изученные связи?")) ==
            QMessageBox::Yes) {
            store_.reset_learning();
            refreshHistory();
        }
    });
    connect(exportButton, &QPushButton::clicked, this, [this]() { exportData(); });
    connect(importButton, &QPushButton::clicked, this, [this]() { importData(); });
    tabs->addTab(historyPage, QStringLiteral("История и данные"));

    auto* diagnosticsPage = new QWidget(tabs);
    auto* diagnosticsLayout = new QVBoxLayout(diagnosticsPage);
    diagnostics_ = new QListWidget(diagnosticsPage);
    diagnosticsLayout->addWidget(diagnostics_);
    auto* clearDiagnostics = new QPushButton(QStringLiteral("Очистить диагностику"), diagnosticsPage);
    diagnosticsLayout->addWidget(clearDiagnostics, 0, Qt::AlignLeft);
    connect(clearDiagnostics, &QPushButton::clicked, this, [this]() {
        store_.clear_diagnostics();
        refreshDiagnostics();
    });
    tabs->addTab(diagnosticsPage, QStringLiteral("Диагностика"));

    auto* aboutPage = new QWidget(tabs);
    auto* aboutLayout = new QVBoxLayout(aboutPage);
    auto* aboutTitle = new QLabel(QStringLiteral("SmartType"), aboutPage);
    QFont aboutFont = aboutTitle->font();
    aboutFont.setPointSize(aboutFont.pointSize() + 4);
    aboutFont.setBold(true);
    aboutTitle->setFont(aboutFont);
    aboutLayout->addWidget(aboutTitle);

    auto* aboutText = new QLabel(
        QStringLiteral("Контекстный автокорректор для Linux. Проект распространяется "
                       "на условиях GNU GPL-3.0."),
        aboutPage);
    aboutText->setWordWrap(true);
    aboutLayout->addWidget(aboutText);

    auto* supportText = new QLabel(
        QStringLiteral("На странице проекта можно сообщить о проблеме, предложить улучшение "
                       "или поддержать дальнейшую разработку."),
        aboutPage);
    supportText->setWordWrap(true);
    supportText->setStyleSheet(QStringLiteral("color: palette(mid);"));
    aboutLayout->addWidget(supportText);

    auto* projectButton = new QPushButton(QStringLiteral("Открыть проект на GitHub"), aboutPage);
    connect(projectButton, &QPushButton::clicked, aboutPage, []() {
        QDesktopServices::openUrl(QUrl(QString::fromLatin1(kProjectUrl)));
    });
    aboutLayout->addWidget(projectButton, 0, Qt::AlignLeft);

    auto* projectLink = new QLabel(
        QStringLiteral("<a href=\"%1\">github.com/Mekhanic/smart_type</a>")
            .arg(QString::fromLatin1(kProjectUrl)),
        aboutPage);
    projectLink->setOpenExternalLinks(true);
    projectLink->setTextInteractionFlags(Qt::TextBrowserInteraction);
    aboutLayout->addWidget(projectLink);
    aboutLayout->addStretch();
    tabs->addTab(aboutPage, QStringLiteral("О проекте"));
    tabs->setCurrentIndex(qBound(0, initial_tab, tabs->count() - 1));

    // Cancel must not persist CandidatePanelScale. Apply/OK call SetConfig.
    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Apply | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("Закрыть"));
    connect(buttons, &QDialogButtonBox::accepted, this, &SettingsDialog::onOk);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttons->button(QDialogButtonBox::Apply), &QPushButton::clicked, this,
            &SettingsDialog::onApply);
    root->addWidget(buttons);

    refreshApplications();
    refreshDictionary();
    refreshBlocked();
    refreshHistory();
    refreshDiagnostics();
}

int SettingsDialog::loadPanelScale() {
    QVariantMap map;
    QString error;
    if (!fetchSmarttypeUiConfig(&map, &error)) {
        return -1;
    }
    return parsePanelScaleValue(map.value(QString::fromLatin1(kCandidatePanelScaleKey)));
}

bool SettingsDialog::applyPanelScale(int value) {
    value = snapPanelScale(value);

    QVariantMap map;
    QString error;
    if (!fetchSmarttypeUiConfig(&map, &error)) {
        if (panelScaleStatus_) {
            panelScaleStatus_->setText(QStringLiteral("Fcitx 5 не запущен"));
            panelScaleStatus_->show();
        }
        setPanelScaleUi(panelScale_ ? panelScale_->value() : kPanelScaleDefault, /*enabled=*/false);
        panelScaleAvailable_ = false;
        return false;
    }

    map.insert(QString::fromLatin1(kCandidatePanelScaleKey), QString::number(value));
    if (!pushSmarttypeUiConfig(map, &error)) {
        QMessageBox::warning(this, QStringLiteral("Размер панели"),
                             QStringLiteral("Fcitx 5 не запущен"));
        return false;
    }

    QVariantMap verifyMap;
    if (!fetchSmarttypeUiConfig(&verifyMap, &error)) {
        QMessageBox::warning(this, QStringLiteral("Размер панели"),
                             QStringLiteral("Не удалось проверить сохранённое значение."));
        return false;
    }
    const int saved =
        parsePanelScaleValue(verifyMap.value(QString::fromLatin1(kCandidatePanelScaleKey)));
    if (saved != value) {
        QMessageBox::warning(
            this, QStringLiteral("Размер панели"),
            QStringLiteral("Сохранённое значение (%1%) не совпадает с запрошенным (%2%).")
                .arg(saved)
                .arg(value));
        return false;
    }

    panelScaleLoaded_ = snapPanelScale(saved);
    setPanelScaleUi(panelScaleLoaded_, /*enabled=*/true);
    if (panelScaleStatus_) panelScaleStatus_->hide();
    return true;
}

void SettingsDialog::setPanelScaleUi(int value, bool enabled) {
    value = snapPanelScale(value);
    if (panelScale_) {
        const QSignalBlocker blocker(panelScale_);
        panelScale_->setValue(value);
        panelScale_->setEnabled(enabled);
    }
    if (panelScaleValue_) {
        panelScaleValue_->setText(QStringLiteral("%1%").arg(value));
        panelScaleValue_->setEnabled(enabled);
    }
}

void SettingsDialog::onPanelScaleSliderMoved(int value) {
    // Preview only — do not call SetConfig here.
    const int snapped = snapPanelScale(value);
    if (panelScale_ && panelScale_->value() != snapped) {
        const QSignalBlocker blocker(panelScale_);
        panelScale_->setValue(snapped);
    }
    if (panelScaleValue_) {
        panelScaleValue_->setText(QStringLiteral("%1%").arg(snapped));
    }
}

void SettingsDialog::onApply() {
    if (!panelScaleAvailable_ || !panelScale_) return;
    const int value = snapPanelScale(panelScale_->value());
    if (value == panelScaleLoaded_) return;
    applyPanelScale(value);
}

void SettingsDialog::onOk() {
    if (panelScaleAvailable_ && panelScale_) {
        const int value = snapPanelScale(panelScale_->value());
        if (value != panelScaleLoaded_) {
            if (!applyPanelScale(value)) {
                return;  // keep dialog open on failure
            }
        }
    }
    accept();
}

void SettingsDialog::refreshApplications() {
    applications_->clear();
    for (const auto& app : store_.blacklist_get()) applications_->addItem(QString::fromStdString(app));
    if (applicationsEmptyLabel_) applicationsEmptyLabel_->setVisible(applications_->count() == 0);
}

void SettingsDialog::refreshDictionary() {
    dictionary_->clear();
    for (const auto& word : store_.words()) dictionary_->addItem(QString::fromStdString(word));
    if (dictionaryEmptyLabel_) dictionaryEmptyLabel_->setVisible(dictionary_->count() == 0);
}

void SettingsDialog::refreshBlocked() {
    blocked_->clear();
    for (const auto& rule : store_.disabled_rules()) {
        auto* item = new QListWidgetItem(QString::fromStdString(rule.typo) + QStringLiteral(" → ") +
                                         QString::fromStdString(rule.correction), blocked_);
        item->setData(Qt::UserRole, QString::fromStdString(rule.typo));
        item->setData(Qt::UserRole + 1, QString::fromStdString(rule.correction));
    }
    if (blockedEmptyLabel_) blockedEmptyLabel_->setVisible(blocked_->count() == 0);
}

void SettingsDialog::refreshHistory() {
    history_->clear();
    for (const auto& entry : store_.history(100)) {
        const auto prefix = entry.undone ? QStringLiteral("↶ ") : QString();
        history_->addItem(prefix + QString::fromStdString(entry.original) + QStringLiteral(" → ") +
                          QString::fromStdString(entry.replacement) +
                          (entry.app.empty() ? QString() : QStringLiteral("  ·  ") +
                           QString::fromStdString(entry.app)));
    }

    const auto stats = store_.get_stats();
    const int successful = stats.total_corrections - stats.undone_corrections;
    const double accuracy = stats.total_corrections > 0
        ? (static_cast<double>(successful) / stats.total_corrections * 100.0)
        : 100.0;
    const int saved_keys = successful * 6;

    statsLabel_->setText(
        QStringLiteral("Всего автоисправлений: %1  |  Отменено (Undo): %2  |  Точность: %3%  |  Сэкономлено нажатий: ~%4")
            .arg(stats.total_corrections)
            .arg(stats.undone_corrections)
            .arg(accuracy, 0, 'f', 1)
            .arg(saved_keys)
    );
}

void SettingsDialog::refreshDiagnostics() {
    diagnostics_->clear();
    for (const auto& entry : store_.diagnostics(200)) {
        diagnostics_->addItem(QString::fromStdString(entry.original) + QStringLiteral(" → ") +
                              QString::fromStdString(entry.candidate) + QStringLiteral("\n") +
                              QString::fromStdString(entry.action) + QStringLiteral(" · ") +
                              QString::number(entry.confidence, 'f', 2) + QStringLiteral(" · ") +
                              QString::fromStdString(entry.reason));
    }
}

void SettingsDialog::exportData() {
    const auto path = QFileDialog::getSaveFileName(this, QStringLiteral("Экспорт SmartType"),
                                                   QStringLiteral("smarttype-dictionary.json"),
                                                   QStringLiteral("JSON (*.json)"));
    if (path.isEmpty()) return;
    QJsonObject root;
    root.insert(QStringLiteral("version"), 1);
    QJsonArray words;
    for (const auto& word : store_.words()) words.append(QString::fromStdString(word));
    root.insert(QStringLiteral("words"), words);
    QJsonArray apps;
    for (const auto& app : store_.blacklist_get()) apps.append(QString::fromStdString(app));
    root.insert(QStringLiteral("excluded_apps"), apps);
    QJsonArray blocked;
    for (const auto& rule : store_.disabled_rules()) {
        QJsonObject item;
        item.insert(QStringLiteral("original"), QString::fromStdString(rule.typo));
        item.insert(QStringLiteral("replacement"), QString::fromStdString(rule.correction));
        blocked.append(item);
    }
    root.insert(QStringLiteral("never_correct"), blocked);
    QFile file(path);
    if (file.open(QIODevice::WriteOnly)) file.write(QJsonDocument(root).toJson());
}

void SettingsDialog::importData() {
    const auto path = QFileDialog::getOpenFileName(this, QStringLiteral("Импорт SmartType"), {},
                                                   QStringLiteral("JSON (*.json)"));
    if (path.isEmpty()) return;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return;
    const auto document = QJsonDocument::fromJson(file.readAll());
    if (!document.isObject() || document.object().value(QStringLiteral("version")).toInt() != 1) {
        QMessageBox::warning(this, QStringLiteral("Импорт"), QStringLiteral("Неподдерживаемый файл."));
        return;
    }
    const auto root = document.object();
    for (const auto& value : root.value(QStringLiteral("words")).toArray()) {
        if (value.isString() && !value.toString().trimmed().isEmpty())
            store_.add_word(value.toString().toStdString());
    }
    for (const auto& value : root.value(QStringLiteral("excluded_apps")).toArray()) {
        if (value.isString() && !value.toString().trimmed().isEmpty())
            store_.blacklist_add(value.toString().toStdString());
    }
    for (const auto& value : root.value(QStringLiteral("never_correct")).toArray()) {
        const auto item = value.toObject();
        const auto original = item.value(QStringLiteral("original")).toString();
        const auto replacement = item.value(QStringLiteral("replacement")).toString();
        if (!original.isEmpty() && !replacement.isEmpty())
            store_.disable_rule(original.toStdString(), replacement.toStdString());
    }
    refreshApplications();
    refreshDictionary();
    refreshBlocked();
}
