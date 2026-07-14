#pragma once

#include <QObject>
#include <QPointer>
#include <QStringList>

class QLocalServer;
class QLocalSocket;
class QWindow;

class RendererBridge final : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.smarttype.UI")
    Q_PROPERTY(QStringList candidates READ candidates NOTIFY panelChanged)
    Q_PROPERTY(int selectedIndex READ selectedIndex NOTIFY panelChanged)
    Q_PROPERTY(bool panelVisible READ panelVisible NOTIFY panelChanged)
    Q_PROPERTY(int flashId READ flashId NOTIFY panelChanged)
    Q_PROPERTY(QString correction READ correction NOTIFY panelChanged)
    Q_PROPERTY(int targetX READ targetX NOTIFY positionChanged)
    Q_PROPERTY(int targetY READ targetY NOTIFY positionChanged)
    Q_PROPERTY(int targetBelowY READ targetBelowY NOTIFY positionChanged)
    Q_PROPERTY(int windowX READ windowX NOTIFY positionChanged)
    Q_PROPERTY(int windowY READ windowY NOTIFY positionChanged)
    Q_PROPERTY(QVariantList flashes READ flashes NOTIFY flashesChanged)
 
public:
    explicit RendererBridge(QObject* parent = nullptr);
 
    [[nodiscard]] QStringList candidates() const { return candidates_; }
    [[nodiscard]] int selectedIndex() const { return selectedIndex_; }
    [[nodiscard]] bool panelVisible() const { return panelVisible_; }
    [[nodiscard]] int flashId() const { return flashId_; }
    [[nodiscard]] QString correction() const { return correction_; }
    [[nodiscard]] int targetX() const { return targetX_; }
    [[nodiscard]] int targetY() const { return targetY_; }
    [[nodiscard]] int targetBelowY() const { return targetBelowY_; }
    [[nodiscard]] int windowX() const { return windowX_; }
    [[nodiscard]] int windowY() const { return windowY_; }
    [[nodiscard]] QVariantList flashes() const { return flashes_; }
 
    Q_INVOKABLE void chooseCandidate(int index);
    Q_INVOKABLE void removeFlash(int id);
    void triggerDemoFlash(const QString& text);
    void setWindow(QWindow* window) { window_ = window; }
 
public slots:
    void SetActiveWindowGeometry(int x, int y, int width, int height);
    void SetActiveWindowGeometry(double x, double y, double width, double height);
 
signals:
    void panelChanged();
    void positionChanged();
    void flashesChanged();

private:
    void readSocket(QLocalSocket* socket);
    void handleMessage(const QByteArray& line);
    void recomputePosition();

    QLocalServer* server_{nullptr};
    QList<QPointer<QLocalSocket>> clients_;
    QStringList candidates_;
    int selectedIndex_{0};
    bool panelVisible_{false};
    int cursorX_{0};
    int cursorY_{0};
    int cursorHeight_{0};
    int composingWidth_{0};
    int windowX_{0};
    int windowY_{0};
    int windowWidth_{0};
    int windowHeight_{0};
    bool haveWindowGeometry_{false};
    int targetX_{0};
    int targetY_{0};
    int targetBelowY_{0};
    int flashId_{0};
    QString correction_;
    QVariantList flashes_;
    QWindow* window_{nullptr};
    QString program_;
    QString frontendName_;
};
