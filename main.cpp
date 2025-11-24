#include "OdaCommon.h"
#include "mainwindow.h"
#include "MyServices.h" // On inclut notre nouvelle classe
#include <QApplication>
#include "StaticRxObject.h"

// doit être dans le cpp où se trouve odInitialize

// Headers requis pour les macros statiques
#include <RxDynamicModule.h>
#include <OdModuleNames.h>

// 1. DÉCLARATIONS
ODRX_DECLARE_STATIC_MODULE_ENTRY_POINT(OdRecomputeDimBlockModule);
ODRX_DECLARE_STATIC_MODULE_ENTRY_POINT(BitmapModule);
ODRX_DECLARE_STATIC_MODULE_ENTRY_POINT(WinOpenGLModule);
ODRX_DECLARE_STATIC_MODULE_ENTRY_POINT(ModelerModule);      // CRITIQUE pour les plans avec 3D
ODRX_DECLARE_STATIC_MODULE_ENTRY_POINT(ExRasterModule);     // CRITIQUE pour les images/logos
ODRX_DECLARE_STATIC_MODULE_ENTRY_POINT(OdRasterProcessingServicesImpl);

// 2. MAP
ODRX_BEGIN_STATIC_MODULE_MAP()
ODRX_DEFINE_STATIC_APPMODULE(OdWinOpenGLModuleName, WinOpenGLModule)
ODRX_DEFINE_STATIC_APPMODULE(OdWinBitmapModuleName, BitmapModule)
ODRX_DEFINE_STATIC_APPMODULE(OdRecomputeDimBlockModuleName, OdRecomputeDimBlockModule)
// Modules additionnels pour éviter les crashs sur entités complexes
ODRX_DEFINE_STATIC_APPMODULE(OdModelerGeometryModuleName, ModelerModule)
ODRX_DEFINE_STATIC_APPMODULE(RX_RASTER_SERVICES_APPNAME, ExRasterModule)
ODRX_DEFINE_STATIC_APPMODULE(OdRasterProcessorModuleName, OdRasterProcessingServicesImpl)
ODRX_END_STATIC_MODULE_MAP()


int main(int argc, char *argv[])
{
    // Permet à Qt de gérer correctement les écrans à haute résolution (HiDPI)
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);

    QApplication a(argc, argv);

    // --- Initialisation de Teigha ---
    // avant odInitialize
    // Enregistre les modules déclarés plus haut (comme RecomputeDimBlock)
    ODRX_INIT_STATIC_MODULE_MAP();

    // On utilise OdStaticRxObject pour créer l'objet sur la pile (stack)
    // Cela évite les erreurs "cannot instantiate abstract class"
    OdStaticRxObject<MyServices> services;
    g_pServices = &services; // On stocke l'adresse pour l'utiliser ailleurs

    odInitialize(&services);

    int result = 0;
    // --- Lancement de l'application Qt ---
    { // dans un bloc de portée pour être sûr que la destruction de Mainwindow se fasse avant odUninitialize
        MainWindow w;
        w.show();

        result = a.exec();
    }

    // --- Nettoyage de Teigha ---
    odUninitialize();

    g_pServices = nullptr;

    return result;
}
