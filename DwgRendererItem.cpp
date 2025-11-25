#include "DwgRendererItem.h"
#include "Ge/GeExtents3d.h"
#include <OdaCommon.h>

#include <QDebug>
#include <QGraphicsSceneWheelEvent>
#include <QPainter>
#include <QDir>
#include <QFile>
#include <QWidget>

#include "DbGsManager.h"
#include "GiContextForDbDatabase.h"
#include "OdModuleNames.h"
#include "RxDynamicModule.h"
#include "RxRasterServices.h"
#include "RxVariantValue.h"


DwgRendererItem::DwgRendererItem(OdDbDatabasePtr pDb, QGraphicsItem* parent)
    : QGraphicsObject(parent)
    , m_pDb(pDb)
{
    m_gsDeviceModuleName = OdWinBitmapModuleName;
    setAcceptHoverEvents(true);
    setFlag(QGraphicsItem::ItemIsFocusable);
    setFlag(QGraphicsItem::ItemIsSelectable);
}

DwgRendererItem::~DwgRendererItem()
{
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

bool DwgRendererItem::generateImage(const QSize& size)
{
    if (m_imageGenerated) {
        return true;  // Généré une seule fois
    }

    if (m_pDb.isNull() || size.width() < 1 || size.height() < 1) {
        return false;
    }

    qDebug() << "Generating DWG image at size:" << size;

    try {
        // Créer device bitmap
        OdGsModulePtr pGsModule = odrxDynamicLinker()->loadModule(m_gsDeviceModuleName);
        if (pGsModule.isNull()) {
            qWarning() << "Failed to load GS module";
            return false;
        }

        OdGsDevicePtr pDevice = pGsModule->createDevice();
        if (pDevice.isNull()) {
            qWarning() << "Failed to create device";
            return false;
        }

        // Configuration
        OdRxDictionaryPtr pProps = pDevice->properties();
        if (!pProps.isNull()) {
            if (pProps->has(OD_T("BitPerPixel")))
                pProps->putAt(OD_T("BitPerPixel"), OdRxVariantValue(OdUInt32(24)));
        }

        pDevice->setBackgroundColor(ODRGB(255, 255, 255));

        // Contexte
        OdGiContextForDbDatabasePtr pGiCtx = OdGiContextForDbDatabase::createObject();
        pGiCtx->setDatabase(m_pDb);

        // Setup layout
        OdGsLayoutHelperPtr pHelper = OdDbGsManager::setupActiveLayoutViews(pDevice, pGiCtx);

        if (pHelper.isNull()) {
            qWarning() << "Failed to setup layout helper";
            return false;
        }

        // Taille - utilisez une grande résolution pour meilleure qualité au zoom
        int renderWidth = size.width() * 2;   // 2x pour meilleure qualité
        int renderHeight = size.height() * 2;

        OdGsDCRect rect(0, renderWidth, renderHeight, 0);
        pHelper->onSize(rect);

        // RENDU
        // Essayer d'obtenir le snapshot directement
        OdGiRasterImagePtr pRasImg;
        pDevice->getSnapShot(pRasImg, rect);

        if (!pRasImg.isNull()) {
            qDebug() << "Raster image obtained, size:" << pRasImg->pixelWidth() << "x" << pRasImg->pixelHeight();

            // Convertir directement en QImage sans passer par un fichier
            int width = (int)pRasImg->pixelWidth();
            int height = (int)pRasImg->pixelHeight();
            int bpp = (int)pRasImg->colorDepth();

            if (width > 0 && height > 0) {
                QImage::Format fmt = (bpp == 32) ? QImage::Format_RGB32 : QImage::Format_RGB888;
                m_cachedImage = QImage(width, height, fmt);

                OdUInt32 scnLnSize = pRasImg->scanLineSize();
                OdUInt32 bytesPerPixel = (bpp + 7) / 8;
                OdUInt32 rowSize = width * bytesPerPixel;

                // Copier les scanlines
                for (int y = 0; y < height; ++y) {
                    const OdUInt8* srcLine = pRasImg->scanLines() + y * scnLnSize;
                    OdUInt8* dstLine = m_cachedImage.scanLine(y);
                    memcpy(dstLine, srcLine, qMin((OdUInt32)m_cachedImage.bytesPerLine(), rowSize));
                }

                // RGB -> BGR swap
                m_cachedImage = m_cachedImage.rgbSwapped();
                m_imageGenerated = true;
                qDebug() << "Image generated successfully";
                return true;
            }
        }

        qWarning() << "Failed to get snapshot from device";

        qWarning() << "Failed to export raster image";

    } catch (const OdError& e) {
        qWarning() << "Generation error:" << QString::fromStdWString((const wchar_t*)e.description().c_str());
    } catch (...) {
        qWarning() << "Unknown generation error";
    }

    return false;
}

void DwgRendererItem::paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget* widget)
{
    if (m_pDb.isNull() || !widget) return;

    if (!m_imageGenerated) {
        if (!generateImage(widget->size())) {
            // Fallback
            painter->fillRect(widget->rect(), QColor(240, 240, 240));
            painter->setPen(Qt::red);
            painter->drawText(widget->rect(), Qt::AlignCenter, "Failed to render DWG");
            return;
        }
    }

    if (!m_cachedImage.isNull()) {
        // Utiliser SmoothTransformation pour meilleur rendu au zoom
        painter->setRenderHint(QPainter::SmoothPixmapTransform, true);
        painter->drawImage(widget->rect(), m_cachedImage);
    }
}

void DwgRendererItem::wheelEvent(QGraphicsSceneWheelEvent* event)
{
    // Le zoom sera géré par QGraphicsView
    // On laisse passer l'événement au parent
    event->ignore();
}
