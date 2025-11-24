bool DwgRendererItem::initializeGsDevice(QWidget* viewport)
{
    if (m_isDeviceInitialized || m_pDb.isNull()) return false;
    if (!viewport) return false;

    try {
        OdGsModulePtr pGsModule = odrxDynamicLinker()->loadModule(m_gsDeviceModuleName);
        if (pGsModule.isNull()) return false;

        m_pDevice = pGsModule->createDevice();
        if (m_pDevice.isNull()) return false;

        OdRxDictionaryPtr pProperties = m_pDevice->properties();
        if (!pProperties.isNull())
        {
            // Configuration Palette et propriétés pour WinBitmap Offscreen
            int numColors = 0;
            m_pDevice->getLogicalPalette(numColors);
            if (numColors == 0) {
                m_pDevice->setLogicalPalette(odcmAcadPalette(ODRGB(0,0,0)), 256);
            }

            if (pProperties->has(OD_T("WindowHWND"))) pProperties->putAt(OD_T("WindowHWND"), OdRxVariantValue((OdIntPtr)0));
            if (pProperties->has(OD_T("WindowHDC"))) pProperties->putAt(OD_T("WindowHDC"), OdRxVariantValue((OdIntPtr)0));
            if (pProperties->has(OD_T("DoubleBufferEnabled"))) pProperties->putAt(OD_T("DoubleBufferEnabled"), OdRxVariantValue(bool(false)));
            if (pProperties->has(OD_T("BitPerPixel"))) pProperties->putAt(OD_T("BitPerPixel"), OdRxVariantValue(OdUInt32(24)));

            m_pDevice->setBackgroundColor(ODRGB(0, 0, 0));
        }

        // Taille initiale
        OdGsDCRect gsRect(0, viewport->width(), 0, viewport->height());
        m_pDevice->onSize(gsRect);

        // Contexte
        m_pGiCtx = OdGiContextForDbDatabase::createObject();
        m_pGiCtx->setDatabase(m_pDb);
        m_pGiCtx->enableGsModel(false);

        // --- CONFIGURATION MANUELLE DE LA VUE ---
        OdGsViewPtr pView = m_pDevice->createView();

        // Ajout de la vue au device
        m_pDevice->addView(pView);

        // CORRECTION : Récupérer le BlockTableRecord du layout actif et utiliser .get() pour obtenir le pointeur brut
        OdDbBlockTableRecordPtr pBTR = m_pDb->getActiveLayoutBTRId().safeOpenObject();
        
        // Ajouter le BTR à la vue (utiliser .get() pour convertir le smart pointer en raw pointer)
        pView->add(pBTR.get(), 0);

        pView->setUserGiContext(m_pGiCtx);

        // On n'utilise plus le helper
        m_pLayoutHelper = nullptr;

        m_isDeviceInitialized = true;
        m_firstResize = true;

    } catch (const OdError& e) {
        qWarning() << "Init GS Error:" << QString::fromWCharArray((const wchar_t*)e.description().c_str());
        return false;
    }
    return true;
}