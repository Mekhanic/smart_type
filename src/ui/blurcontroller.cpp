#include "blurcontroller.hpp"

#include <KWindowEffects>
#include <QPainterPath>
#include <QPolygon>
#include <QRectF>
#include <QRegion>
#include <QVariantMap>
#include <QWindow>

void BlurController::updateBlur(QWindow* window, const QVariantList& rectangles) const {
    if (!window) return;

    QRegion blurRegion;
    for (const QVariant& value : rectangles) {
        const QVariantMap data = value.toMap();
        const QRectF rect(data.value(QStringLiteral("x")).toReal(),
                          data.value(QStringLiteral("y")).toReal(),
                          data.value(QStringLiteral("width")).toReal(),
                          data.value(QStringLiteral("height")).toReal());
        const qreal radius = data.value(QStringLiteral("radius")).toReal();
        if (!rect.isValid()) continue;
        QPainterPath path;
        path.addRoundedRect(rect, radius, radius);
        blurRegion += QRegion(path.toFillPolygon().toPolygon());
    }
    KWindowEffects::enableBlurBehind(window, !blurRegion.isEmpty(), blurRegion);
}
