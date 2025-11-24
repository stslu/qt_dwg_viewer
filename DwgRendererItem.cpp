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
    if(m_pDb.isNull())
        return;

    if(widget->width() < 2 || widget->height() < 2)
        return;

    if(!m_isDeviceInitialized) {
        if(!initializeGsDevice(widget))
            return;
    }

    if(m_pDevice.isNull())
        return;

    try {
        // 1. Redimensionnement
        OdGsDCRect gsRect(0, widget->width(), 0, widget->height());
        m_pDevice->onSize(gsRect);

        // 2. Récupération de la vue (créée manuellement dans initializeGsDevice)
        OdGsViewPtr pView = m_pDevice->viewAt(0);
        if(pView.isNull())
            return;

        if(m_firstResize) {
            // Mode Filaire 2D Optimisé
            pView->setMode(OdGsView::k2DOptimized);

            ensureExtentsValid();
            OdGePoint3d min(m_cachedBoundingRect.left(), m_cachedBoundingRect.top(), 0);
            OdGePoint3d max(m_cachedBoundingRect.right(), m_cachedBoundingRect.bottom(), 0);

            pView->zoomExtents(min, max);
            pView->zoom(0.95);

            m_firstResize = false;
        }

        // 3. Rendu
        m_pDevice->update();

        // 4. Image
        OdRxObjectPtr pRasObj   = m_pDevice->properties()->getAt(OD_T("RasterImage")); // génère Paint Error Code: 5  Desc: "Invalid input"
        OdGiRasterImagePtr pRas = OdGiRasterImage::cast(pRasObj);

        if(!pRas.isNull()) {
            int width  = (int) pRas->pixelWidth();
            int height = (int) pRas->pixelHeight();
            int bpp    = (int) pRas->colorDepth();

            QImage::Format fmt = QImage::Format_Invalid;
            if(bpp == 32)
                fmt = QImage::Format_RGB32;
            else if(bpp == 24)
                fmt = QImage::Format_RGB888;

            if(fmt != QImage::Format_Invalid && width > 0 && height > 0) {
                // Get Teigha's scanline size (may include padding/alignment)
                OdUInt32 scnLnSize = pRas->scanLineSize();
                // Convert bits-per-pixel to bytes-per-pixel (round up to nearest byte)
                OdUInt32 bytesPerPixel = (bpp + 7) / 8;
                OdUInt32 rowSize       = width * bytesPerPixel;

                // Create QImage
                QImage img(width, height, fmt);
                bool imageCopied = false;

                // Check if we can copy directly or need to handle stride differences
                if(scnLnSize == rowSize && scnLnSize == (OdUInt32) img.bytesPerLine()) {
                    // Direct copy - strides match
                    pRas->scanLines(img.bits(), 0, height);
                    imageCopied = true;
                } else {
                    // Handle stride mismatch - use temporary buffer
                    // Check for potential overflow (ensure buffer size fits in OdUInt32)
                    if(scnLnSize > 0 && height > 0 && (OdUInt64) scnLnSize * (OdUInt64) height <= (OdUInt64) 0xFFFFFFFF) {
                        OdUInt32 teighaBufferSize = scnLnSize * height;

                        // Allocate temporary buffer for Teigha's format
                        QByteArray teighaBuffer(teighaBufferSize, 0);
                        pRas->scanLines((OdUInt8*) teighaBuffer.data(), 0, height);

                        // Copy line by line to handle stride differences
                        for(int y = 0; y < height; ++y) {
                            const OdUInt8* srcLine = (const OdUInt8*) teighaBuffer.data() + y * scnLnSize;
                            OdUInt8* dstLine       = img.scanLine(y);
                            memcpy(dstLine, srcLine, rowSize);
                        }

                        imageCopied = true;
                    } else {
                        qWarning() << "Image too large: potential overflow. Size:" << width << "x" << height << "ScanLineSize:" << scnLnSize;
                    }
                }

                if(imageCopied) {
                    painter->drawImage(widget->rect(), img.rgbSwapped());
                }
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
    if(m_isDeviceInitialized || m_pDb.isNull())
        return false;
    if(!viewport)
        return false;

    try {
        OdGsModulePtr pGsModule = odrxDynamicLinker()->loadModule(m_gsDeviceModuleName);
        if(pGsModule.isNull())
            return false;

        m_pDevice = pGsModule->createDevice();
        if(m_pDevice.isNull())
            return false;

        OdRxDictionaryPtr pProperties = m_pDevice->properties();
        if(!pProperties.isNull()) {
            // Configuration Palette et propriétés pour WinBitmap Offscreen
            int numColors = 0;
            m_pDevice->getLogicalPalette(numColors);
            if(numColors == 0) {
                m_pDevice->setLogicalPalette(odcmAcadPalette(ODRGB(0, 0, 0)), 256);
            }

            if(pProperties->has(OD_T("WindowHWND")))
                pProperties->putAt(OD_T("WindowHWND"), OdRxVariantValue((OdIntPtr) 0));
            if(pProperties->has(OD_T("WindowHDC")))
                pProperties->putAt(OD_T("WindowHDC"), OdRxVariantValue((OdIntPtr) 0));
            if(pProperties->has(OD_T("DoubleBufferEnabled")))
                pProperties->putAt(OD_T("DoubleBufferEnabled"), OdRxVariantValue(bool(false)));
            if(pProperties->has(OD_T("BitPerPixel")))
                pProperties->putAt(OD_T("BitPerPixel"), OdRxVariantValue(OdUInt32(24)));

            m_pDevice->setBackgroundColor(ODRGB(0, 0, 0));
        }

        // Taille initiale
        OdGsDCRect gsRect(0, viewport->width(), 0, viewport->height());
        m_pDevice->onSize(gsRect);

        // Contexte
        m_pGiCtx = OdGiContextForDbDatabase::createObject();
        m_pGiCtx->setDatabase(m_pDb);
        m_pGiCtx->enableGsModel(false);

        // --- CONFIGURATION MANUELLE DE LA VUE (Bypass OdDbGsManager) ---
        OdGsViewPtr pView = m_pDevice->createView();

        // Ajout de la vue au device
        m_pDevice->addView(pView);

        // CORRECTION: Récupérer le BlockTableRecord du layout actif
        OdDbBlockTableRecordPtr pBTR = m_pDb->getActiveLayoutBTRId().safeOpenObject();
        // Ajouter le BTR à la vue (utiliser .get() pour obtenir le pointeur brut)
        pView->add(pBTR.get(), 0);

        pView->setUserGiContext(m_pGiCtx);

        // On n'utilise plus le helper
        m_pLayoutHelper = nullptr;

        m_isDeviceInitialized = true;
        m_firstResize         = true;

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
