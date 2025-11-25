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

// Windows only
#include <windows.h>

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
    //WINDOWS ONLY
    if(m_pDb.isNull()) return;
    if(widget->width() < 2 || widget->height() < 2) return;

    if(!m_isDeviceInitialized) {
        if(!initializeGsDevice(widget))
            return;
    }

    if(m_pDevice.isNull()) return;

    try {
        // Redimensionner la fenêtre offscreen si nécessaire
        if (!createOffscreenWindow(widget->width(), widget->height())) return;

        // Mettre à jour la taille du device
        OdGsDCRect gsRect(0, widget->width(), widget->height(), 0);
        m_pDevice->onSize(gsRect);

        if(m_firstResize && !m_pLayoutHelper.isNull()) {
            m_pLayoutHelper->onSize(gsRect);
            m_firstResize = false;
        }

        // Rendu
        qDebug().noquote() << "-----1";
        m_pDevice->update();
        qDebug().noquote() << "-----2";

        // Copier le DIB vers QImage
        if (m_pBits) {
            int stride = ((m_bitmapWidth * 3 + 3) & ~3);
            QImage img((uchar*)m_pBits, m_bitmapWidth, m_bitmapHeight,
                       stride, QImage::Format_RGB888);
            painter->drawImage(widget->rect(), img.rgbSwapped());
        }

    } catch(const OdError& e) {
        qDebug() << "Paint Error Code:" << e.code()
        << " Desc:" << QString::fromStdWString((const wchar_t*)e.description().c_str());
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
    //WINDOWS ONLY
    if(m_isDeviceInitialized || m_pDb.isNull()) return false;
    if(!viewport) return false;

    try {
        // Créer la fenêtre offscreen
        if (!createOffscreenWindow(viewport->width(), viewport->height())) {
            return false;
        }

        OdGsModulePtr pGsModule = odrxDynamicLinker()->loadModule(OdWinGDIModuleName);
        if (pGsModule.isNull()) return false;

        m_pDevice = pGsModule->createDevice();
        if (m_pDevice.isNull()) return false;

        OdRxDictionaryPtr pProps = m_pDevice->properties();
        if (!pProps.isNull()) {
            pProps->putAt(OD_T("WindowHWND"), OdRxVariantValue((OdIntPtr)m_hOffscreenWnd));
            pProps->putAt(OD_T("WindowHDC"), OdRxVariantValue((OdIntPtr)m_hOffscreenDC));
        }

        m_pDevice->setBackgroundColor(ODRGB(255, 255, 255));

        m_pGiCtx = OdGiContextForDbDatabase::createObject();
        m_pGiCtx->setDatabase(m_pDb);

        m_pLayoutHelper = OdDbGsManager::setupActiveLayoutViews(m_pDevice, m_pGiCtx);

        if (m_pLayoutHelper.isNull()) return false;

        OdGsDCRect gsRect(0, viewport->width(), viewport->height(), 0);
        m_pDevice->onSize(gsRect);

        m_isDeviceInitialized = true;
        m_firstResize = true;

    } catch(const OdError& e) {
        qWarning() << "Init GS Error:" << QString::fromStdWString((const wchar_t*)e.description().c_str());
        return false;
    }
    return true;

}

void DwgRendererItem::destroyGsDevice()
{
    //WINDOWS ONLY
    destroyGsDevice();
    destroyOffscreenWindow();
}



//------------------------WINDOWS ONLY
bool DwgRendererItem::createOffscreenWindow(int width, int height)
{
    if (m_hOffscreenWnd && m_bitmapWidth == width && m_bitmapHeight == height) {
        return true;
    }

    destroyOffscreenWindow();

    // Créer une fenêtre invisible
    m_hOffscreenWnd = CreateWindowExW(
        0, L"STATIC", L"TeighaOffscreen",
        WS_POPUP,
        0, 0, width, height,
        nullptr, nullptr, GetModuleHandle(nullptr), nullptr
        );

    if (!m_hOffscreenWnd) {
        qWarning() << "Failed to create offscreen window";
        return false;
    }

    m_hOffscreenDC = GetDC(m_hOffscreenWnd);
    if (!m_hOffscreenDC) {
        DestroyWindow(m_hOffscreenWnd);
        m_hOffscreenWnd = nullptr;
        return false;
    }

    // Créer le DIB
    BITMAPINFO bmi;
    ZeroMemory(&bmi, sizeof(BITMAPINFO));
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = width;
    bmi.bmiHeader.biHeight = -height;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 24;
    bmi.bmiHeader.biCompression = BI_RGB;

    m_hBitmap = CreateDIBSection(m_hOffscreenDC, &bmi, DIB_RGB_COLORS, &m_pBits, nullptr, 0);
    if (!m_hBitmap) {
        qWarning() << "Failed to create DIB section";
        ReleaseDC(m_hOffscreenWnd, m_hOffscreenDC);
        DestroyWindow(m_hOffscreenWnd);
        m_hOffscreenWnd = nullptr;
        m_hOffscreenDC = nullptr;
        return false;
    }

    m_hOldBitmap = (HBITMAP)SelectObject(m_hOffscreenDC, m_hBitmap);
    m_bitmapWidth = width;
    m_bitmapHeight = height;

    return true;
}

void DwgRendererItem::destroyOffscreenWindow()
{
    if (m_hBitmap) {
        if (m_hOffscreenDC && m_hOldBitmap) {
            SelectObject(m_hOffscreenDC, m_hOldBitmap);
        }
        DeleteObject(m_hBitmap);
        m_hBitmap = nullptr;
        m_hOldBitmap = nullptr;
    }

    if (m_hOffscreenDC) {
        ReleaseDC(m_hOffscreenWnd, m_hOffscreenDC);
        m_hOffscreenDC = nullptr;
    }

    if (m_hOffscreenWnd) {
        DestroyWindow(m_hOffscreenWnd);
        m_hOffscreenWnd = nullptr;
    }

    m_pBits = nullptr;
    m_bitmapWidth = 0;
    m_bitmapHeight = 0;
}
