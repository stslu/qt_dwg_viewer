#include "DwgViewerWidget.h"
#include <QPaintEvent>
#include <QResizeEvent>
#include <QWheelEvent>
#include <QDebug>
#include <cmath>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

#include "RxDynamicModule.h"
#include "OdModuleNames.h"
#include "RxVariantValue.h"

DwgViewerWidget::DwgViewerWidget(QWidget *parent)
    : QWidget(parent)
    , m_isInitialized(false)
    , m_zoomFactor(1.0)
{
    // Optimisations critiques pour le rendu direct WinGDI avec HWND Windows
    setAttribute(Qt::WA_PaintOnScreen, true);      // Force direct painting to screen, bypassing Qt's backing store (required for native HWND rendering)
    setAttribute(Qt::WA_NoSystemBackground, true);  // Prevents Qt from erasing background, reduces flickering with native rendering
    setAttribute(Qt::WA_OpaquePaintEvent, true);    // Widget fills entire area, Qt doesn't need to erase first (optimization for WinGDI)
    
    // Taille minimale raisonnable
    setMinimumSize(400, 300);
}

DwgViewerWidget::~DwgViewerWidget()
{
    destroyDevice();
}

void DwgViewerWidget::setDatabase(OdDbDatabasePtr pDb)
{
    if (m_pDb == pDb)
        return;

    destroyDevice();
    m_pDb = pDb;
    m_isInitialized = false;

    // Forcer la réinitialisation au prochain paint
    update();
}

bool DwgViewerWidget::initializeDevice()
{
    if (m_isInitialized || m_pDb.isNull())
        return false;

    try {
        // Charger le module WinGDI
        OdGsModulePtr pGsModule = odrxDynamicLinker()->loadModule(OdWinGDIModuleName);
        if (pGsModule.isNull()) {
            qWarning() << "Failed to load WinGDI module";
            return false;
        }

        m_pDevice = pGsModule->createDevice();
        if (m_pDevice.isNull()) {
            qWarning() << "Failed to create device";
            return false;
        }

        // CRITIQUE: Utiliser le HWND réel du widget Qt
        OdRxDictionaryPtr pProps = m_pDevice->properties();
        if (!pProps.isNull()) {
#ifdef Q_OS_WIN
            pProps->putAt(OD_T("WindowHWND"), OdRxVariantValue((OdIntPtr)winId()));
#endif
        }

        // Fond blanc
        m_pDevice->setBackgroundColor(ODRGB(255, 255, 255));

        // Créer le contexte Gi
        m_pGiCtx = OdGiContextForDbDatabase::createObject();
        m_pGiCtx->setDatabase(m_pDb);

        // Setup du layout actif
        m_pLayoutHelper = OdDbGsManager::setupActiveLayoutViews(m_pDevice, m_pGiCtx);
        
        if (m_pLayoutHelper.isNull()) {
            qWarning() << "Failed to setup layout helper";
            return false;
        }

        // Configurer la taille initiale
        updateDevice();

        m_isInitialized = true;
        qDebug() << "DwgViewerWidget: Device initialized successfully";
        return true;

    } catch (const OdError& e) {
        qWarning() << "Init Error:" << QString::fromStdWString((const wchar_t*)e.description().c_str());
        return false;
    }
}

void DwgViewerWidget::destroyDevice()
{
    m_pLayoutHelper.release();
    m_pGiCtx.release();
    m_pDevice.release();
    m_isInitialized = false;
}

void DwgViewerWidget::updateDevice()
{
    if (m_pDevice.isNull())
        return;

    try {
        int w = width();
        int h = height();
        
        if (w < 1 || h < 1)
            return;

        OdGsDCRect rect(0, w, h, 0);
        m_pDevice->onSize(rect);

    } catch (const OdError& e) {
        qWarning() << "Update device error:" << QString::fromStdWString((const wchar_t*)e.description().c_str());
    }
}

void DwgViewerWidget::paintEvent(QPaintEvent *)
{
    if (!m_isInitialized) {
        if (!initializeDevice()) {
            qWarning() << "Failed to initialize device in paintEvent";
            return;
        }
    }
    
    if (m_pDevice.isNull())
        return;
    
    try {
        // Rendu DIRECT sur le HWND Windows
        m_pDevice->update();
    } catch (const OdError& e) {
        qWarning() << "Paint Error:" << e.code() 
                   << QString::fromStdWString((const wchar_t*)e.description().c_str());
    }
}

void DwgViewerWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    
    if (m_isInitialized) {
        updateDevice();
        update();
    }
}

void DwgViewerWidget::wheelEvent(QWheelEvent *event)
{
    if (!m_isInitialized || m_pDevice.isNull()) {
        QWidget::wheelEvent(event);
        return;
    }

    try {
        // Vérifier qu'il y a au moins une vue
        if (m_pDevice->numViews() == 0) {
            QWidget::wheelEvent(event);
            return;
        }

        // Calculer le facteur de zoom
        double delta = event->angleDelta().y();
        double zoomDelta = delta > 0 ? 1.15 : 1.0 / 1.15;
        
        // Protection contre division par zéro (bien qu'improbable)
        static constexpr double MIN_ZOOM_THRESHOLD = 1e-6;
        if (std::abs(zoomDelta) < MIN_ZOOM_THRESHOLD) {
            event->accept();
            return;
        }
        
        m_zoomFactor *= zoomDelta;

        // Obtenir la vue active
        OdGsView* pView = m_pDevice->viewAt(0);
        if (pView) {
            // Zoomer autour du centre de la vue
            OdGePoint3d target = pView->target();
            OdGeVector3d viewDir = pView->position() - target;
            
            // Ajuster la position de la caméra
            pView->setView(
                target + viewDir / zoomDelta,
                target,
                pView->upVector(),
                pView->fieldWidth() / zoomDelta,
                pView->fieldHeight() / zoomDelta
            );

            update();
        }

        event->accept();
    } catch (const OdError& e) {
        qWarning() << "Wheel event error:" << QString::fromStdWString((const wchar_t*)e.description().c_str());
        QWidget::wheelEvent(event);
    }
}
