#include <OdaCommon.h> // TOUJOURS EN PREMIER

#ifndef MYSERVICES_H
#define MYSERVICES_H

#include "ExSystemServices.h"
#include "ExHostAppServices.h"

// Classe de services combinant les services système et l'hôte d'application
class MyServices : public ExSystemServices, public ExHostAppServices
{
protected:
    ODRX_USING_HEAP_OPERATORS(ExSystemServices);

public:
    // Déclaration de la surcharge pour le mode statique
    virtual OdGsDevicePtr gsBitmapDevice(OdRxObject* pViewObj = NULL,
                                         OdDbBaseDatabase* pDb = NULL,
                                         OdUInt32 flags = 0);
};

// Déclaration externe du pointeur global pour qu'il soit visible partout
extern MyServices* g_pServices;

#endif // MYSERVICES_H
