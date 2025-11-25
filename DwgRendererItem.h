#include "OdaCommon.h"

#ifndef DWGRENDERERITEM_H
#define DWGRENDERERITEM_H

#include <QGraphicsObject>
#include <QImage>
#include <QCursor>

#include "DbDatabase.h"
// #include "Gs/Gs.h"
// #include "DbGsManager.h"
// #include "GiContextForDbDatabase.h"

class DwgRendererItem : public QGraphicsObject
{
    Q_OBJECT

public:
    DwgRendererItem(OdDbDatabasePtr pDb, QGraphicsItem *parent = nullptr);
    ~DwgRendererItem();

    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

protected:
    void wheelEvent(QGraphicsSceneWheelEvent *event) override;

private:
    OdDbDatabasePtr m_pDb;
    OdString m_gsDeviceModuleName;

    QImage m_cachedImage;
    bool m_imageGenerated = false;

    mutable QRectF m_cachedBoundingRect;
    mutable bool m_bExtentsCalculated = false;

    void ensureExtentsValid() const;
    bool generateImage(const QSize& size);
};

#endif // DWGRENDERERITEM_H
