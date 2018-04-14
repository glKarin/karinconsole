TEMPLATE        = app
#DESTDIR         = ..
DESTDIR         = ./

CONFIG          += qt debug_and_release warn_on # build_all
CONFIG          += meegotouch

CONFIG += link_pkgconfig
PKGCONFIG += maliit-1.0

QT += core gui

MOC_DIR         = ../.moc

DEFINES += APP_NAME=\\\"Karin-Console\\\"
DEFINES += APP_NAME_SETTINGS=\\\"karin-console\\\"
DEFINES += DEVELOPER=\\\"Karin\\\"
DEFINES += VERSION=\\\"0.2.5harmattan3\\\"

CONFIG(debug, debug|release) {
    OBJECTS_DIR = ../.objs_d
    TARGET      = karin-console_d
		LIBS            += -L../lib -lkonsole -lX11 -lmeegotouchviews
		#LIBS            += -L../lib ../lib/libkonsole_d.a -lX11 -lmeegotouchviews
    PRE_TARGETDEPS += ../lib/libkonsole_d.a
		DEFINES += _KARIN_LOCAL_
		DEFINES += _KARIN_PREFIX_=\\\".\\\"
} else {
    OBJECTS_DIR = ../.objs
    TARGET      = karin-console
		LIBS            += -L../lib -lkonsole -lX11 -lmeegotouchviews
    PRE_TARGETDEPS += ../lib/libkonsole.so
		DEFINES += _KARIN_INSTALL_
		DEFINES += _KARIN_PREFIX_=\\\"/usr/share/karin-console\\\"
}

LMTP_HEADERS    = lmtp/mtopleveloverlay.h lmtp/meditortoolbararrow.h \
                  lmtp/meditortoolbar_p.h lmtp/meditortoolbar.h
LMTP_SOURCES    = lmtp/mtopleveloverlay.cpp lmtp/meditortoolbararrow.cpp \
                  lmtp/meditortoolbar.cpp

HEADERS         = tab_model.h button_with_label.h tab_group.h karin_ut.h tab_button.h terminal.h tab_bar.h MTermWidget.h MTerminalDisplay.h $$LMTP_HEADERS
SOURCES         = main.cpp tab_model.cpp button_with_label.cpp tab_group.cpp karin_ut.cpp tab_button.cpp terminal.cpp tab_bar.cpp MTermWidget.cpp MTerminalDisplay.cpp $$LMTP_SOURCES

INCLUDEPATH     = ../lib ./lmtp

#LIBS           += -L.. -lqgraphicstermwidget

desktop_entry.path = $$[QT_INSTALL_PREFIX]/share/applications
desktop_entry.files = karin-console.desktop

imtoolbars.path = $$[QT_INSTALL_PREFIX]/share/karin-console/toolbars
imtoolbars.files = toolbars/shell.xml toolbars/arrows.xml toolbars/function.xml toolbars/karin.xml

target.path = $$[QT_INSTALL_PREFIX]/bin

style.files = style/karin_mstyle.css
style.path = $$[QT_INSTALL_PREFIX]/share/karin-console/style

i18n.files = i18n/karin-console.zh_CN.ts i18n/karin-console.zh_CN.qm
i18n.path = $$[QT_INSTALL_PREFIX]/share/karin-console/i18n

rsc.files = resource/toolbarxml.html
rsc.path = $$[QT_INSTALL_PREFIX]/share/karin-console/resource

icon.files = karin-console.png
icon.path = $$[QT_INSTALL_PREFIX]/share/karin-console/icon

INSTALLS        += target desktop_entry imtoolbars style i18n rsc icon
