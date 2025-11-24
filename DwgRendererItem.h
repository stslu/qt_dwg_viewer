#include "OdaCommon.h"

#ifndef DWGRENDERERITEM_H
#define DWGRENDERERITEM_H

#include <QGraphicsItem>
#include <QGraphicsObject>
#include <QCursor>

#include "DbDatabase.h"
#include "Gs/Gs.h"
#include "DbGsManager.h"
#include "GiContextForDbDatabase.h"

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
    bool initializeGsDevice(QWidget* viewport);
    void destroyGsDevice();

    // Méthode interne pour calculer les extents une seule fois
    void ensureExtentsValid() const;

    OdDbDatabasePtr m_pDb;
    OdGsDevicePtr m_pDevice;
    OdGsLayoutHelperPtr m_pLayoutHelper;

    // --- CORRECTION CRITIQUE : Le contexte doit être un membre pour ne pas être détruit ---
    OdGiContextForDbDatabasePtr m_pGiCtx;

    bool m_isDeviceInitialized = false;
    bool m_firstResize = true;

    // Cache pour les performances et la stabilité
    mutable QRectF m_cachedBoundingRect;
    mutable bool m_bExtentsCalculated = false;

    OdString m_gsDeviceModuleName;
};

#endif // DWGRENDERERITEM_H
