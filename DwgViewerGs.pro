QT += core gui widgets openglwidgets

CONFIG += c++17 teigha
TARGET = DwgViewerGs

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    DwgRendererItem.cpp \
    MyServices.cpp

HEADERS += \
    mainwindow.h \
    DwgRendererItem.h \
    MyServices.h

# ===================================================================
# Configuration de Teigha - À ADAPTER À VOTRE SYSTÈME
# ===================================================================
# Remplacez "C:/Teigha_26.10" par le chemin racine de votre installation Teigha.
# TEIGHA_PATH = "C:/Teigha_26.10"

# # Module de rendu OpenGL. Assurez-vous que le nom du module est correct pour votre version.
# # Pour les versions récentes sur Windows, c'est généralement WinOpenGL.
# # Pourrait être "WinDirectX", "MacOpenGL", etc. sur d'autres plateformes.
# TEIGHA_GS_MODULE = "WinOpenGL.txv"

# # Chemins vers les en-têtes (Headers)
# INCLUDEPATH += $$TEIGHA_PATH/Kernel/Include
# INCLUDEPATH += $$TEIGHA_PATH/Drawing/Include
# INCLUDEPATH += $$TEIGHA_PATH/Drawing/Extensions/ExServices

# # Chemins vers les bibliothèques (Libs) pour Windows / Visual Studio 2022
# LIBS += -L$$TEIGHA_PATH/lib/vc17_amd64_16.0
# LIBS += -L$$TEIGHA_PATH/thirdparty/lib/vc17_amd64_16.0

# Bibliothèques Teigha requises
# LIBS += -lTD_Alloc.lib \
#         -lTD_Db.lib \
#         -lTD_DbRoot.lib \
#         -lTD_ExamplesCommon.lib \
#         -lTD_Ge.lib \
#         -lTD_Gis.lib \
#         -lTD_Gi.lib \
#         -lTD_Root.lib \
#         -lTD_SpatialIndex.lib

# # Dépendances tierces
# LIBS += -lFreeImage.lib \
#         -lfreetype.lib

# Copie des DLLs et modules nécessaires dans le répertoire de build
# win32 {
#     QMAKE_POST_LINK += copy /Y "$$replace(TEIGHA_PATH, /, \\)\\Kernel\\dll\\vc17_amd64_16.0\\TD_Alloc.dll" "$$replace(OUT_PWD, /, \\)" $$escape_expand(\\n\\t)
#     QMAKE_POST_LINK += copy /Y "$$replace(TEIGHA_PATH, /, \\)\\Drawing\\dll\\vc17_amd64_16.0\\TD_Db.dll" "$$replace(OUT_PWD, /, \\)" $$escape_expand(\\n\\t)
#     QMAKE_POST_LINK += copy /Y "$$replace(TEIGHA_PATH, /, \\)\\Platforms\\vc17_amd64_16.0\\$$TEIGHA_GS_MODULE" "$$replace(OUT_PWD, /, \\)" $$escape_expand(\\n\\t)
# }
