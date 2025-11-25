#include "OdaCommon.h"

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QWheelEvent>

// Includes Teigha
#include "DbDatabase.h"

// Includes du projet
#include "DwgViewerWidget.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void openDwgFile();

protected:
    void wheelEvent(QWheelEvent* event) override;


private:
    void setupUi();

    QGraphicsScene* m_scene;
    QGraphicsView* m_view;

    // Pointeur intelligent vers la base de données DWG actuellement chargée
    OdDbDatabasePtr m_pDb;
};

#endif // MAINWINDOW_H
