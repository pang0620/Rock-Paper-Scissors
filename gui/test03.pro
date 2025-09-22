QT       += core gui widgets network

CONFIG += c++17

# OpenCV 헤더 경로
INCLUDEPATH += /usr/include/opencv4

# OpenCV 라이브러리 링크
LIBS += -L/usr/lib/x86_64-linux-gnu \
        -lopencv_core -lopencv_highgui -lopencv_videoio -lopencv_imgproc -lopencv_imgcodecs

SOURCES += \
    main.cpp \
    mainwidget.cpp

HEADERS += \
    mainwidget.h
2
FORMS += \
    mainwidget.ui
