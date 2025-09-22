QT       += core gui widgets charts webenginewidgets

CONFIG += c++17 link_pkgconfig
PKGCONFIG += opencv4

SOURCES += \
    main.cpp \
    mainwidget.cpp

HEADERS += \
    mainwidget.h

FORMS += \
    mainwidget.ui
