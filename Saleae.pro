#-------------------------------------------------
#
# Project created by QtCreator 2015-06-24T08:36:07
#
#-------------------------------------------------

QT       += core gui
QT       += network

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = Saleae
TEMPLATE = app


SOURCES += main.cpp\
        saleae.cpp

HEADERS  += saleae.h

FORMS    += saleae.ui

QMAKE_LFLAGS_WINDOWS = /SUBSYSTEM:WINDOWS,5.01
QMAKE_MAC_SDK = macosx10.12
