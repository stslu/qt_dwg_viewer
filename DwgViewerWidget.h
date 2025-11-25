#ifndef DWGVIEWERWIDGET_H
#define DWGVIEWERWIDGET_H

#include "OdaCommon.h"

#include <QWidget>
#include "DbDatabase.h"
#include "Gs/Gs.h"
#include "DbGsManager.h"
#include "GiContextForDbDatabase.h"

class DwgViewerWidget : public QWidget
{
    Q_OBJECT

public:
    explicit DwgViewerWidget(QWidget *parent = nullptr);
    ~DwgViewerWidget();

    void setDatabase(OdDbDatabasePtr pDb);

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    bool initializeDevice();
    void destroyDevice();
    void updateDevice();

    OdDbDatabasePtr m_pDb;
    OdGsDevicePtr m_pDevice;
    OdGiContextForDbDatabasePtr m_pGiCtx;
    OdGsLayoutHelperPtr m_pLayoutHelper;

    bool m_isInitialized;
    double m_zoomFactor;
};

#endif // DWGVIEWERWIDGET_H
