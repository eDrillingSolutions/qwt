# -*- mode: sh -*- ################################################
# Qwt Widget Library
# Copyright (C) 1997   Josef Wilgen
# Copyright (C) 2002   Uwe Rathmann
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the Qwt License, Version 1.0
###################################################################

QWT_ROOT = ../..

include( $${QWT_ROOT}/qwtconfig.pri )

SUFFIX_STR =
CONFIG(debug, debug|release) {
    SUFFIX_STR = $${DEBUG_SUFFIX}
}
else {
    SUFFIX_STR = $${RELEASE_SUFFIX}
}

CONFIG += no_keywords

TEMPLATE     = app

MOC_DIR      = moc
INCLUDEPATH += $${QWT_ROOT}/src
DEPENDPATH  += $${QWT_ROOT}/src
OBJECTS_DIR  = obj$${SUFFIX_STR}
DESTDIR      = $${QWT_ROOT}/examples/bin$${SUFFIX_STR}

QWTLIB       = qwt$${SUFFIX_STR}

win32 {
    contains(CONFIG, QwtDll) {
        DEFINES    += QT_DLL QWT_DLL
        QWTLIB = $${QWTLIB}$${VER_MAJ}
    }

    msvc:LIBS  += $${QWT_ROOT}/lib/$${QWTLIB}.lib
    msvc.net:LIBS  += $${QWT_ROOT}/lib/$${QWTLIB}.lib
    msvc2002:LIBS += $${QWT_ROOT}/lib/$${QWTLIB}.lib
    msvc2003:LIBS += $${QWT_ROOT}/lib/$${QWTLIB}.lib
    msvc2005:LIBS += $${QWT_ROOT}/lib/$${QWTLIB}.lib
    msvc2008:LIBS += $${QWT_ROOT}/lib/$${QWTLIB}.lib
    g++:LIBS   += -L$${QWT_ROOT}/lib -l$${QWTLIB}
}
else {
    LIBS        += -L$${QWT_ROOT}/lib -l$${QWTLIB}
}
