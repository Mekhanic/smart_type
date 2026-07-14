#pragma once

#include <QObject>
#include <QVariantList>

class QWindow;

class BlurController final : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;

    Q_INVOKABLE void updateBlur(QWindow* window, const QVariantList& rectangles) const;
};
