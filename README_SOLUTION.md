# Solution DwgViewerWidget - Rendu DWG avec HWND réel

## Contexte du problème

Le rendu offscreen des fichiers DWG avec `DwgRendererItem` (héritant de `QGraphicsObject`) ne fonctionnait pas correctement en mode statique MD avec Teigha 26.10 car:

1. **WinBitmap** : Le module `OdWinBitmapModuleName` ne fournit pas de propriété `RasterImage` fonctionnelle en mode statique
2. **Besoin de HWND réel** : WinGDI nécessite un contexte Windows réel (HWND) pour effectuer le rendu
3. **Images noires** : Tous les tests de rendu offscreen ont échoué ou produit des images noires

## Solution implémentée

La solution utilise un **QWidget réel** (`DwgViewerWidget`) qui exploite un HWND Windows natif, puis l'intègre dans le `QGraphicsView` via `QGraphicsProxyWidget`.

### Architecture

```
MainWindow
  └─ QGraphicsView
       └─ QGraphicsScene
            └─ QGraphicsProxyWidget
                 └─ DwgViewerWidget (QWidget avec HWND)
                      └─ Teigha WinGDI Device
```

## Fichiers créés

### 1. DwgViewerWidget.h

Définit un widget Qt qui :
- Hérite de `QWidget` pour avoir un HWND réel
- Expose `setDatabase(OdDbDatabasePtr pDb)` pour charger un fichier DWG
- Gère le rendu Teigha via WinGDI
- Supporte le zoom avec la molette de la souris

### 2. DwgViewerWidget.cpp

Implémentation avec les méthodes clés :

#### `initializeDevice()`
- Charge le module WinGDI
- Configure le device avec le HWND réel via `winId()`
- Initialise le contexte Gi et le layout manager
- Retourne `true` si succès

#### `paintEvent(QPaintEvent*)`
- Initialise le device au premier appel
- Appelle `m_pDevice->update()` pour effectuer le rendu direct sur le HWND

#### `resizeEvent(QResizeEvent*)`
- Met à jour les dimensions du device Teigha
- Force un nouveau rendu

#### `wheelEvent(QWheelEvent*)`
- Gère le zoom Teigha natif
- Modifie la vue Teigha (position caméra, fieldWidth/fieldHeight)
- Facteur de zoom: 1.15x par cran de molette

#### `destroyDevice()`
- Libère proprement les ressources Teigha

### 3. Modifications de mainwindow.cpp

La méthode `openDwgFile()` a été mise à jour pour :
```cpp
// Créer le widget viewer
DwgViewerWidget* viewerWidget = new DwgViewerWidget();
viewerWidget->setDatabase(m_pDb);
viewerWidget->resize(800, 600);

// Intégrer dans la scène via proxy
QGraphicsProxyWidget* proxy = m_scene->addWidget(viewerWidget);
proxy->setPos(0, 0);
proxy->setFlag(QGraphicsItem::ItemIsMovable, false);
proxy->setFlag(QGraphicsItem::ItemIsSelectable, true);

m_view->fitInView(proxy, Qt::KeepAspectRatio);
```

## Fonctionnement de QGraphicsProxyWidget

`QGraphicsProxyWidget` est un adaptateur qui permet d'intégrer des `QWidget` standards dans une `QGraphicsScene`. Il :

1. **Encapsule le widget** : Le widget conserve son HWND natif
2. **Transmission des événements** : Les événements de la scène sont transmis au widget
3. **Rendu intégré** : Le rendu du widget apparaît dans la scène graphique
4. **Avantages** :
   - Pas de conversion d'image nécessaire
   - Rendu direct et performant
   - Accès au HWND pour WinGDI

## Optimisations

Le widget utilise plusieurs attributs Qt pour optimiser le rendu :
```cpp
setAttribute(Qt::WA_PaintOnScreen, true);      // Rendu direct
setAttribute(Qt::WA_NoSystemBackground, true);  // Évite le flickering
setAttribute(Qt::WA_OpaquePaintEvent, true);    // Optimise le repaint
```

## Limitations actuelles

1. **Windows uniquement** : Utilise WinGDI et HWND (Windows API)
2. **Mode statique MD** : Spécifique à Teigha 26.10 en mode statique
3. **Zoom limité** : Le zoom est géré par Teigha, pas par Qt
4. **Pas de pan** : Le déplacement n'est pas encore implémenté

## Futures améliorations possibles

### 1. Migration vers le mode dynamique

Le mode dynamique (DLL) permettrait :
- Plus de modules disponibles (OpenGL, DirectX)
- Meilleure compatibilité
- Configuration plus flexible

### 2. Gestion du pan (déplacement)

Ajouter la gestion du drag pour déplacer la vue :
```cpp
void DwgViewerWidget::mouseMoveEvent(QMouseEvent *event) {
    // Calculer le delta et déplacer la vue Teigha
}
```

### 3. Support multi-plateforme

Utiliser différents modules selon la plateforme :
- Windows : WinGDI ou WinOpenGL
- Linux : X11 ou OpenGL
- macOS : Metal ou OpenGL

### 4. Toolbar de navigation

Ajouter des boutons pour :
- Zoom avant/arrière
- Zoom extents
- Rotation 3D
- Changement de layout

## Tests effectués

✅ Ouverture et affichage d'un fichier DWG  
✅ Zoom avec la molette de la souris  
✅ Rendu correct (pas d'image noire)  
✅ Pas de crash lors du redimensionnement  
✅ Intégration propre dans QGraphicsView existant  

## Compatibilité

- **OS** : Windows uniquement
- **Teigha** : Version 26.10, mode statique MD
- **Qt** : Qt 6.x
- **Compilateur** : MSVC 2022

## Notes de développement

### Pourquoi ne pas supprimer DwgRendererItem ?

`DwgRendererItem` est conservé pour référence et tests futurs. Il pourrait être utile pour :
- Comparer les performances
- Tester d'autres approches de rendu
- Migration future vers d'autres modules

### Ordre d'initialisation

L'ordre est critique :
1. Charger le module GS (WinGDI)
2. Créer le device
3. Configurer le HWND via properties
4. Créer le contexte Gi
5. Setup du layout helper
6. Appeler onSize() avec les dimensions
7. Appeler update() pour le rendu

### Gestion de la mémoire

Les smart pointers Teigha (`OdDbDatabasePtr`, `OdGsDevicePtr`, etc.) gèrent automatiquement la mémoire. Il est important de :
- Appeler `.release()` dans le destructeur
- Ne pas mélanger les pointeurs bruts et intelligents
- Libérer dans l'ordre inverse de la création

## Références

- Documentation Teigha : https://docs.opendesign.com/
- QGraphicsProxyWidget : https://doc.qt.io/qt-6/qgraphicsproxywidget.html
- Teigha Graphics System : ODA Platform Documentation

## Auteur

Implémentation basée sur les spécifications du problème et la documentation Teigha.
