#include <OdaCommon.h> // TOUJOURS EN PREMIER
#include "MyServices.h"

// Nécessaire pour accéder aux noms des modules (OdWinBitmapModuleName)
#include "RxDynamicModule.h"
#include "OdModuleNames.h"

MyServices* g_pServices = nullptr;

// Implémentation de la fonction requise en statique
OdGsDevicePtr MyServices::gsBitmapDevice(OdRxObject* /*pViewObj*/,
                                         OdDbBaseDatabase* /*pDb*/,
                                         OdUInt32 /*flags*/)
{
    try
    {
        // En mode statique, loadModule va chercher dans la map définie dans main.cpp
        // Si WinBitmapModule y est enregistré, cela renverra le pointeur vers le module statique.
        OdGsModulePtr pGsModule = ::odrxDynamicLinker()->loadModule(OdWinBitmapModuleName);

        if (!pGsModule.isNull())
            return pGsModule->createBitmapDevice();
    }
    catch(const OdError&)
    {
        // Gestion silencieuse ou log
    }
    return OdGsDevicePtr();
}
