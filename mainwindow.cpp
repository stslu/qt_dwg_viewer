#include "mainwindow.h"
#include "DwgRendererItem.h"

#include <QToolBar>
#include <QPushButton>
#include <QFileDialog>
#include <QMessageBox>
#include <QOpenGLWidget>
#include <QSurfaceFormat>

#include "MyServices.h"

// // Includes Teigha pour charger le fichier
// #include "Extensions/ExServices/ExSystemServices.h"
// #include "Extensions/ExServices/ExHostAppServices.h"
// #include "Extensions/ExServices/OdFileBuf.h"


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setupUi();
}

MainWindow::~MainWindow()
{
    // Le OdDbDatabasePtr se nettoie automatiquement
    // Forcer la libération de la base de données avant la fermeture
    m_pDb.release();
}

void MainWindow::setupUi()
{
    setWindowTitle("Visionneuse DWG Hybride");
    setMinimumSize(1024, 768);

    m_scene = new QGraphicsScene(this);
    m_view = new QGraphicsView(m_scene);

    // Configuration OpenGL
    QOpenGLWidget* openGlWidget = new QOpenGLWidget();
    QSurfaceFormat format;
    format.setSamples(4);
    format.setDepthBufferSize(24); // Important pour la 3D
    format.setStencilBufferSize(8);
    openGlWidget->setFormat(format);

    m_view->setViewport(openGlWidget);
    m_view->setRenderHint(QPainter::Antialiasing, true);
    // m_view->setDragMode(QGraphicsView::ScrollHandDrag);
    m_view->setDragMode(QGraphicsView::NoDrag);
    m_view->setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    m_view->setViewportUpdateMode(QGraphicsView::FullViewportUpdate); // Important pour OpenGL

    setCentralWidget(m_view);

    QToolBar* toolBar = addToolBar("Fichier");
    QPushButton* openButton = new QPushButton("Ouvrir un fichier DWG", this);
    connect(openButton, &QPushButton::clicked, this, &MainWindow::openDwgFile);
    toolBar->addWidget(openButton);
}

void MainWindow::openDwgFile()
{
    QString filePath = QFileDialog::getOpenFileName(this, "Ouvrir", "", "Fichiers AutoCAD (*.dwg)");
    if (filePath.isEmpty()) return;

    m_scene->clear();
    m_pDb.release();

    try
    {
        if (!g_pServices) throw std::runtime_error("Services Teigha non initialisés.");

        // Utilisation du service global pour lire le fichier
        // Cast explicite vers wchar_t* pour être sûr
        m_pDb = g_pServices->readFile((const wchar_t*)filePath.toStdWString().c_str());

        if (m_pDb.isNull()) throw std::runtime_error("Impossible de lire le fichier DWG.");
    }
    catch (const OdError& e)
    {
        QString msg = QString::fromWCharArray((const wchar_t*)e.description().c_str());
        QMessageBox::critical(this, "Erreur Teigha", "Erreur de chargement :\n" + msg);
        return;
    }
    catch (const std::exception& ex)
    {
        QMessageBox::critical(this, "Erreur", QString("Erreur système : %1").arg(ex.what()));
        return;
    }

    DwgRendererItem* dwgItem = new DwgRendererItem(m_pDb);
    m_scene->addItem(dwgItem);

    QGraphicsSimpleTextItem* annotation = new QGraphicsSimpleTextItem("Annotation Qt");
    annotation->setPos(0, 0);
    annotation->setBrush(Qt::red);
    annotation->setFont(QFont("Arial", 50, QFont::Bold));
    m_scene->addItem(annotation);
}
