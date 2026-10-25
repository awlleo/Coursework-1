QT += core gui widgets sql
CONFIG += c++17
TARGET = trips

SOURCES += main.cpp \
           database.cpp \
           mainwindow.cpp \
           tableform.cpp \
           tripform.cpp

HEADERS += database.h \
           mainwindow.h \
           tableform.h \
           tripform.h
