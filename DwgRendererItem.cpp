#include "DwgRendererItem.h"
#include "Ge/GeExtents3d.h"
#include "OdRound.h"
#include <OdaCommon.h>

#include <QByteArray>
#include <QDebug>
#include <QGraphicsSceneWheelEvent>
#include <QOpenGLWidget>
#include <QPainter>
#include <cstring>

#include "ColorMapping.h"
#include "DbBlockTableRecord.h"
#include "DbGsManager.h"
#include "Gi/GiRasterImage.h"
#include "GiContextForDbDatabase.h"
#include "OdModuleNames.h"
#include "RxDynamicModule.h"
#include "RxVariantValue.h"

DwgRendererItem::DwgRendererItem(OdDbDatabasePtr pDb, QGraphicsItem* parent)
    : QGraphicsObject(parent)
    , m_pDb(pDb)
{
    m_gsDeviceModuleName = OdWinBitmapModuleName;
    setAcceptHoverEvents(true);
    setFlag(QGraphicsItem::ItemIsFocusable);
}

DwgRendererItem::~DwgRendererItem()
{
    destroyGsDevice();
}

void DwgRendererItem::ensureExtentsValid() const
{
    if(m_bExtentsCalculated)
        return;

    m_cachedBoundingRect = QRectF(0, 0, 1000, 1000);

    if(!m_pDb.isNull()) {
        try {
            OdGeExtents3d extents;
            if(m_pDb->getGeomExtents(extents) == eOk && extents.isValidExtents()) {
                OdGePoint3d min = extents.minPoint();
                OdGePoint3d max = extents.maxPoint();

                // Anti-crash: si min/max sont trop proches
                if(min.distanceTo(max) < 1.0) {
                    min.x -= 50.0;
                    min.y -= 50.0;
                    max.x += 50.0;
                    max.y += 50.0;
                }

                double w = max.x - min.x;
                double h = max.y - min.y;

                double limit = 10000000.0;
                if(w > limit || h > limit || qAbs(min.x) > limit || qAbs(min.y) > limit) {
                    m_cachedBoundingRect = QRectF(-limit / 2, -limit / 2, limit, limit);
                } else {
                    m_cachedBoundingRect = QRectF(min.x, min.y, w, h);
                }
            }
        } catch(...) {
        }
    }
    m_bExtentsCalculated = true;
}

QRectF DwgRendererItem::boundingRect() const
{
    ensureExtentsValid();
    return m_cachedBoundingRect;
}

void DwgRendererItem::paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget* widget)
{
    if(m_pDb.isNull()) return;
    if(widget->width() < 2 || widget->height() < 2) return;

    if(!m_isDeviceInitialized) {
        if(!initializeGsDevice(widget))
            return;
    }

    if(m_pDevice.isNull() || m_pLayoutHelper.isNull()) return;

    try {
        // 1. Redimensionnement
        OdGsDCRect gsRect(0, widget->width(), widget->height(), 0);
        m_pDevice->onSize(gsRect);

        if(m_firstResize) {
            // Utiliser le helper pour zoomer
            m_pLayoutHelper->onSize(gsRect);
            m_firstResize = false;
        }

        // 2. Rendu
        m_pDevice->update();

        // TEST : Vérifier les capacités du device
        qDebug() << "Device class name:" << QString::fromStdWString((const wchar_t*)m_pDevice->isA()->name().c_str());

        OdRxDictionaryPtr pProps = m_pDevice->properties();
        if (!pProps.isNull()) {
            qDebug() << "Available properties:";
            OdRxDictionaryIteratorPtr pIter = pProps->newIterator();
            for (; !pIter->done(); pIter->next()) {
                qDebug() << "  -" <<  QString::fromWCharArray((const wchar_t*)pIter->getKey().c_str());
            }
        }

        // 3. Récupération de l'image via les properties du DEVICE (pas update)
        OdGiRasterImagePtr pRasImg;
        m_pDevice->getSnapShot(pRasImg, OdGsDCRect(0, widget->width(), widget->height(), 0));

        if(!pRasImg.isNull()) {
            int width = (int)pRasImg->pixelWidth();
            int height = (int)pRasImg->pixelHeight();
            int bpp = (int)pRasImg->colorDepth();

            if(width > 0 && height > 0) {
                QImage::Format fmt = (bpp == 32) ? QImage::Format_RGB32 : QImage::Format_RGB888;
                QImage img(width, height, fmt);

                OdUInt32 scnLnSize = pRasImg->scanLineSize();

                for(int y = 0; y < height; ++y) {
                    const OdUInt8* srcLine = pRasImg->scanLines() + y * scnLnSize;
                    memcpy(img.scanLine(y), srcLine, img.bytesPerLine());
                }

                painter->drawImage(widget->rect(), img.rgbSwapped());
            }
        }

    } catch(const OdError& e) {
        qDebug() << "Paint Error Code:" << e.code() << " Desc:" << QString::fromWCharArray((const wchar_t*) e.description().c_str());
    } catch(...) {
        qDebug() << "Unknown Paint Error";
    }
}

void DwgRendererItem::wheelEvent(QGraphicsSceneWheelEvent* event)
{
    if(m_pDevice.isNull())
        return;

    OdGsViewPtr pView = m_pDevice->viewAt(0);
    if(pView.isNull())
        return;

    double scaleFactor = (event->delta() > 0) ? 1.0 / 1.1 : 1.1;
    pView->zoom(scaleFactor);

    update();
    event->accept();
}

bool DwgRendererItem::initializeGsDevice(QWidget* viewport)
{
    if(m_isDeviceInitialized || m_pDb.isNull()) return false;
    if(!viewport) return false;

    try {
        OdGsModulePtr pGsModule = odrxDynamicLinker()->loadModule(m_gsDeviceModuleName);
        if (pGsModule.isNull()) {
            qWarning() << "Failed to load GS module";
            return false;
        }

        m_pDevice = pGsModule->createDevice();
        if (m_pDevice.isNull()) {
            qWarning() << "Failed to create device";
            return false;
        }

        // IMPORTANT : Pas de configuration de properties AVANT setupActiveLayoutViews !
        m_pDevice->setBackgroundColor(ODRGB(255, 255, 255));

        // Contexte
        m_pGiCtx = OdGiContextForDbDatabase::createObject();
        m_pGiCtx->setDatabase(m_pDb);

        // Utiliser le helper qui configure tout automatiquement
        m_pLayoutHelper = OdDbGsManager::setupActiveLayoutViews(m_pDevice, m_pGiCtx);

        if (m_pLayoutHelper.isNull()) {
            qWarning() << "Failed to setup layout helper";
            return false;
        }

        // MAINTENANT configurer la taille
        OdGsDCRect gsRect(0, viewport->width(), viewport->height(), 0);
        m_pDevice->onSize(gsRect);

        m_isDeviceInitialized = true;
        m_firstResize = true;

    } catch(const OdError& e) {
        qWarning() << "Init GS Error:" << QString::fromWCharArray((const wchar_t*) e.description().c_str());
        return false;
    }
    return true;
}

void DwgRendererItem::destroyGsDevice()
{
    if(!m_isDeviceInitialized)
        return;

    if(!m_pDevice.isNull()) {
        m_pDevice->eraseAllViews();
    }

    m_pLayoutHelper.release();
    m_pDevice.release();
    m_pGiCtx.release();
    m_isDeviceInitialized = false;
}
