#ifndef _PANEL_
#define _PANEL_

#include "settings.h"
#include <qwidget.h>

class QCheckBox;
class QComboBox;
class QSpinBox;

class Panel: public QWidget
{
    Q_OBJECT

public:
    Panel( QWidget *parent = NULL );

    void setSettings( const Settings &);
    Settings settings() const;
    
Q_SIGNALS:
    void edited();

private:
    struct
    {
        QCheckBox *checkBox;
        QComboBox *positionBox;

    } d_legend;

    struct
    {
        QCheckBox *checkBox;
        QSpinBox *numColumnsBox;
        QComboBox *hAlignmentBox;
        QComboBox *vAlignmentBox;
        QComboBox *backgroundBox;

    } d_legendItem;

    QSpinBox *d_numCurves;
};

#endif
