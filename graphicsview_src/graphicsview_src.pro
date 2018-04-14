TEMPLATE        = app
DESTDIR         = ./
#DESTDIR         = ..

CONFIG          += qt debug_and_release warn_on #build_all

QT += core gui

MOC_DIR         = ../.moc
LIBS            += -L..

CONFIG(debug, debug|release) {
    OBJECTS_DIR = ../.objs_d
    TARGET      = konsole-graphicsview_d
    LIBS        += -L../lib ../lib/libkonsole_d.a
    PRE_TARGETDEPS += ../lib/libkonsole_d.a
} else {
    OBJECTS_DIR = ../.objs
    TARGET      = konsole-graphicsview
    LIBS        += -L../lib -lkonsole
    PRE_TARGETDEPS += ../lib/libkonsole.so
}

SOURCES         = main_graphicsview.cpp 

INCLUDEPATH     = ../lib

#LIBS           += -L.. -lqgraphicstermwidget

target.path = $$[QT_INSTALL_PREFIX]/bin

INSTALLS += target
