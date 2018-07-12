/* -*- mode: C++ ; c-file-style: "stroustrup" -*- *****************************
 * Qwt Widget Library
 * Copyright (C) 1997   Josef Wilgen
 * Copyright (C) 2002   Uwe Rathmann
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the Qwt License, Version 1.0
 *****************************************************************************/

#include "qwt_plot_renderer.h"
#include "qwt_plot.h"
#include "qwt_painter.h"
#include "qwt_plot_layout.h"
#include "qwt_abstract_legend.h"
#include "qwt_scale_widget.h"
#include "qwt_scale_engine.h"
#include "qwt_text.h"
#include "qwt_text_label.h"
#include "qwt_math.h"
#include <qpainter.h>
#include <qpaintengine.h>
#include <qtransform.h>
#include <qprinter.h>
#include <qprintdialog.h>
#include <qfiledialog.h>
#include <qfileinfo.h>
#include <qstyle.h>
#include <qstyleoption.h>
#include <qimagewriter.h>

#ifndef QWT_NO_SVG
#ifdef QT_SVG_LIB
#if QT_VERSION >= 0x040500
#define QWT_FORMAT_SVG 1
#endif
#endif
#endif

#ifndef QT_NO_PRINTER
#define QWT_FORMAT_PDF 1
#endif

#ifndef QT_NO_PDF

// QPdfWriter::setResolution() has been introduced with
// Qt 5.3. Guess it is o.k. to stay with QPrinter for older
// versions.

#if QT_VERSION >= 0x050300

#ifndef QWT_FORMAT_PDF
#define QWT_FORMAT_PDF 1
#endif

#define QWT_PDF_WRITER 1

#endif
#endif

#ifndef QT_NO_PRINTER
// postscript support has been dropped in Qt5
#if QT_VERSION < 0x050000
#define QWT_FORMAT_POSTSCRIPT 1
#endif
#endif

#if QWT_FORMAT_SVG
#include <qsvggenerator.h>
#endif

#if QWT_PDF_WRITER
#include <qpdfwriter.h>
#endif

static QPainterPath qwtCanvasClip(
    const QWidget* canvas, const QRectF &canvasRect )
{
    // The clip region is calculated in integers
    // To avoid too much rounding errors better
    // calculate it in target device resolution

    int x1 = qCeil( canvasRect.left() );
    int x2 = qFloor( canvasRect.right() );
    int y1 = qCeil( canvasRect.top() );
    int y2 = qFloor( canvasRect.bottom() );

    const QRect r( x1, y1, x2 - x1 - 1, y2 - y1 - 1 );

    QPainterPath clipPath;

    ( void ) QMetaObject::invokeMethod(
        const_cast< QWidget *>( canvas ), "borderPath",
        Qt::DirectConnection,
        Q_RETURN_ARG( QPainterPath, clipPath ), Q_ARG( QRect, r ) );

    return clipPath;
}

static inline QFont qwtResolvedFont( const QWidget *widget )
{
    QFont font = widget->font();
    font.resolve( QFont::AllPropertiesResolved );

    return font;
}

class QwtPlotRenderer::PrivateData
{
public:
    PrivateData():
        discardFlags( QwtPlotRenderer::DiscardNone ),
        layoutFlags( QwtPlotRenderer::DefaultLayout )
    {
    }

    QwtPlotRenderer::DiscardFlags discardFlags;
    QwtPlotRenderer::LayoutFlags layoutFlags;
};

/*!
   Constructor
   \param parent Parent object
*/
QwtPlotRenderer::QwtPlotRenderer( QObject *parent ):
    QObject( parent )
{
    d_data = new PrivateData;
}

//! Destructor
QwtPlotRenderer::~QwtPlotRenderer()
{
    delete d_data;
}

/*!
  Change a flag, indicating what to discard from rendering

  \param flag Flag to change
  \param on On/Off

  \sa DiscardFlag, testDiscardFlag(), setDiscardFlags(), discardFlags()
*/
void QwtPlotRenderer::setDiscardFlag( DiscardFlag flag, bool on )
{
    if ( on )
        d_data->discardFlags |= flag;
    else
        d_data->discardFlags &= ~flag;
}

/*!
  \return True, if flag is enabled.
  \param flag Flag to be tested
  \sa DiscardFlag, setDiscardFlag(), setDiscardFlags(), discardFlags()
*/
bool QwtPlotRenderer::testDiscardFlag( DiscardFlag flag ) const
{
    return d_data->discardFlags & flag;
}

/*!
  Set the flags, indicating what to discard from rendering

  \param flags Flags
  \sa DiscardFlag, setDiscardFlag(), testDiscardFlag(), discardFlags()
*/
void QwtPlotRenderer::setDiscardFlags( DiscardFlags flags )
{
    d_data->discardFlags = flags;
}

/*!
  \return Flags, indicating what to discard from rendering
  \sa DiscardFlag, setDiscardFlags(), setDiscardFlag(), testDiscardFlag()
*/
QwtPlotRenderer::DiscardFlags QwtPlotRenderer::discardFlags() const
{
    return d_data->discardFlags;
}

/*!
  Change a layout flag

  \param flag Flag to change
  \param on On/Off

  \sa LayoutFlag, testLayoutFlag(), setLayoutFlags(), layoutFlags()
*/
void QwtPlotRenderer::setLayoutFlag( LayoutFlag flag, bool on )
{
    if ( on )
        d_data->layoutFlags |= flag;
    else
        d_data->layoutFlags &= ~flag;
}

/*!
  \return True, if flag is enabled.
  \param flag Flag to be tested
  \sa LayoutFlag, setLayoutFlag(), setLayoutFlags(), layoutFlags()
*/
bool QwtPlotRenderer::testLayoutFlag( LayoutFlag flag ) const
{
    return d_data->layoutFlags & flag;
}

/*!
  Set the layout flags

  \param flags Flags
  \sa LayoutFlag, setLayoutFlag(), testLayoutFlag(), layoutFlags()
*/
void QwtPlotRenderer::setLayoutFlags( LayoutFlags flags )
{
    d_data->layoutFlags = flags;
}

/*!
  \return Layout flags
  \sa LayoutFlag, setLayoutFlags(), setLayoutFlag(), testLayoutFlag()
*/
QwtPlotRenderer::LayoutFlags QwtPlotRenderer::layoutFlags() const
{
    return d_data->layoutFlags;
}

/*!
  Render a plot to a file

  The format of the document will be auto-detected from the
  suffix of the file name.

  \param plot Plot widget
  \param fileName Path of the file, where the document will be stored
  \param sizeMM Size for the document in millimeters.
  \param resolution Resolution in dots per Inch (dpi)
*/
void QwtPlotRenderer::renderDocument( QwtPlot *plot,
    const QString &fileName, const QSizeF &sizeMM, int resolution )
{
    renderDocument( plot, fileName,
        QFileInfo( fileName ).suffix(), sizeMM, resolution );
}

/*!
  Render a plot to a file

  Supported formats are:

  - pdf\n
    Portable Document Format PDF
  - ps\n
    Postcript
  - svg\n
    Scalable Vector Graphics SVG
  - all image formats supported by Qt\n
    see QImageWriter::supportedImageFormats()

  Scalable vector graphic formats like PDF or SVG are superior to
  raster graphics formats.

  \param plot Plot widget
  \param fileName Path of the file, where the document will be stored
  \param format Format for the document
  \param sizeMM Size for the document in millimeters.
  \param resolution Resolution in dots per Inch (dpi)

  \sa renderTo(), render(), QwtPainter::setRoundingAlignment()
*/
void QwtPlotRenderer::renderDocument( QwtPlot *plot,
    const QString &fileName, const QString &format,
    const QSizeF &sizeMM, int resolution )
{
    if ( plot == NULL || sizeMM.isEmpty() || resolution <= 0 )
        return;

    QString title = plot->title().text();
    if ( title.isEmpty() )
        title = "Plot Document";

    const double mmToInch = 1.0 / 25.4;
    const QSizeF size = sizeMM * mmToInch * resolution;

    const QRectF documentRect( 0.0, 0.0, size.width(), size.height() );

    const QString fmt = format.toLower();
    if ( fmt == QLatin1String( "pdf" ) )
    {
#if QWT_FORMAT_PDF

#if QWT_PDF_WRITER
        QPdfWriter pdfWriter( fileName );
        pdfWriter.setPageSizeMM( sizeMM );
        pdfWriter.setTitle( title );
        pdfWriter.setPageMargins( QMarginsF() );
        pdfWriter.setResolution( resolution ); 
        
        QPainter painter( &pdfWriter );
        render( plot, &painter, documentRect );
#else
        QPrinter printer;
        printer.setOutputFormat( QPrinter::PdfFormat );
        printer.setColorMode( QPrinter::Color );
        printer.setFullPage( true );
        printer.setPaperSize( sizeMM, QPrinter::Millimeter );
        printer.setDocName( title );
        printer.setOutputFileName( fileName );
        printer.setResolution( resolution );

        QPainter painter( &printer );
        render( plot, &painter, documentRect );
#endif
#endif
    }
    else if ( fmt == QLatin1String( "ps" ) )
    {
#if QWT_FORMAT_POSTSCRIPT
        QPrinter printer;
        printer.setOutputFormat( QPrinter::PostScriptFormat );
        printer.setColorMode( QPrinter::Color );
        printer.setFullPage( true );
        printer.setPaperSize( sizeMM, QPrinter::Millimeter );
        printer.setDocName( title );
        printer.setOutputFileName( fileName );
        printer.setResolution( resolution );

        QPainter painter( &printer );
        render( plot, &painter, documentRect );
#endif
    }
    else if ( fmt == QLatin1String( "svg" ) )
    {
#if QWT_FORMAT_SVG
        QSvgGenerator generator;
        generator.setTitle( title );
        generator.setFileName( fileName );
        generator.setResolution( resolution );
        generator.setViewBox( documentRect );

        QPainter painter( &generator );
        render( plot, &painter, documentRect );
#endif
    }
    else
    {
        if ( QImageWriter::supportedImageFormats().indexOf(
            format.toLatin1() ) >= 0 )
        {
            const QRect imageRect = documentRect.toRect();
            const int dotsPerMeter = qRound( resolution * mmToInch * 1000.0 );

            QImage image( imageRect.size(), QImage::Format_ARGB32 );
            image.setDotsPerMeterX( dotsPerMeter );
            image.setDotsPerMeterY( dotsPerMeter );
            image.fill( QColor( Qt::white ).rgb() );

            QPainter painter( &image );
            render( plot, &painter, imageRect );
            painter.end();

            image.save( fileName, format.toLatin1() );
        }
    }
}

/*!
  \brief Render the plot to a \c QPaintDevice

  This function renders the contents of a QwtPlot instance to
  \c QPaintDevice object. The target rectangle is derived from
  its device metrics.

  \param plot Plot to be rendered
  \param paintDevice device to paint on, f.e a QImage

  \sa renderDocument(), render(), QwtPainter::setRoundingAlignment()
*/

void QwtPlotRenderer::renderTo(
    QwtPlot *plot, QPaintDevice &paintDevice ) const
{
    int w = paintDevice.width();
    int h = paintDevice.height();

    QPainter p( &paintDevice );
    render( plot, &p, QRectF( 0, 0, w, h ) );
}

/*!
  \brief Render the plot to a QPrinter

  This function renders the contents of a QwtPlot instance to
  \c QPaintDevice object. The size is derived from the printer
  metrics.

  \param plot Plot to be rendered
  \param printer Printer to paint on

  \sa renderDocument(), render(), QwtPainter::setRoundingAlignment()
*/

#ifndef QT_NO_PRINTER

void QwtPlotRenderer::renderTo(
    QwtPlot *plot, QPrinter &printer ) const
{
    int w = printer.width();
    int h = printer.height();

    QRectF rect( 0, 0, w, h );
    double aspect = rect.width() / rect.height();
    if ( ( aspect < 1.0 ) )
        rect.setHeight( aspect * rect.width() );

    QPainter p( &printer );
    render( plot, &p, rect );
}

#endif

#if QWT_FORMAT_SVG

/*!
  \brief Render the plot to a QSvgGenerator

  If the generator has a view box, the plot will be rendered into it.
  If it has no viewBox but a valid size the target coordinates
  will be (0, 0, generator.width(), generator.height()). Otherwise
  the target rectangle will be QRectF(0, 0, 800, 600);

  \param plot Plot to be rendered
  \param generator SVG generator
*/
void QwtPlotRenderer::renderTo(
    QwtPlot *plot, QSvgGenerator &generator ) const
{
    QRectF rect = generator.viewBoxF();
    if ( rect.isEmpty() )
        rect.setRect( 0, 0, generator.width(), generator.height() );

    if ( rect.isEmpty() )
        rect.setRect( 0, 0, 800, 600 ); // something

    QPainter p( &generator );
    render( plot, &p, rect );
}

#endif

/*!
  Paint the contents of a QwtPlot instance into a given rectangle.

  \param plot Plot to be rendered
  \param painter Painter
  \param plotRect Bounding rectangle

  \sa renderDocument(), renderTo(), QwtPainter::setRoundingAlignment()
*/
void QwtPlotRenderer::render( QwtPlot *plot,
    QPainter *painter, const QRectF &plotRect ) const
{
    if ( painter == 0 || !painter->isActive() ||
            !plotRect.isValid() || plot->size().isNull() )
    {
        return;
    }

    if ( !( d_data->discardFlags & DiscardBackground ) )
        QwtPainter::drawBackgound( painter, plotRect, plot );

    /*
      The layout engine uses the same methods as they are used
      by the Qt layout system. Therefore we need to calculate the
      layout in screen coordinates and paint with a scaled painter.
     */
    QTransform transform;
    transform.scale(
        double( painter->device()->logicalDpiX() ) / plot->logicalDpiX(),
        double( painter->device()->logicalDpiY() ) / plot->logicalDpiY() );

    QRectF layoutRect = transform.inverted().mapRect( plotRect );

    if ( !( d_data->discardFlags & DiscardBackground ) )
    {
        // subtract the contents margins

        int left, top, right, bottom;
        plot->getContentsMargins( &left, &top, &right, &bottom );
        layoutRect.adjust( left, top, -right, -bottom );
    }

    QwtPlotLayout *layout = plot->plotLayout();

    int baseLineDists[QwtPlot::axisCnt];
    int canvasMargins[QwtPlot::axisCnt];

    for ( int axisId = 0; axisId < QwtPlot::axisCnt; axisId++ )
    {
        canvasMargins[ axisId ] = layout->canvasMargin( axisId );

        if ( d_data->layoutFlags & FrameWithScales )
        {
            QwtScaleWidget *scaleWidget = plot->axisWidget( axisId );
            if ( scaleWidget )
            {
                baseLineDists[axisId] = scaleWidget->margin();
                scaleWidget->setMargin( 0 );
            }

            if ( !plot->axisEnabled( axisId ) )
            {
                int left = 0;
                int right = 0;
                int top = 0;
                int bottom = 0;

                // When we have a scale the frame is painted on
                // the position of the backbone - otherwise we
                // need to introduce a margin around the canvas

                switch( axisId )
                {
                    case QwtPlot::yLeft:
                        layoutRect.adjust( 1, 0, 0, 0 );
                        break;
                    case QwtPlot::yRight:
                        layoutRect.adjust( 0, 0, -1, 0 );
                        break;
                    case QwtPlot::xTop:
                        layoutRect.adjust( 0, 1, 0, 0 );
                        break;
                    case QwtPlot::xBottom:
                        layoutRect.adjust( 0, 0, 0, -1 );
                        break;
                    default:
                        break;
                }
                layoutRect.adjust( left, top, right, bottom );
            }
        }
    }

    // Calculate the layout for the document.

    QwtPlotLayout::Options layoutOptions = QwtPlotLayout::IgnoreScrollbars;

    if ( ( d_data->layoutFlags & FrameWithScales ) ||
        ( d_data->discardFlags & DiscardCanvasFrame ) )
    {
        layoutOptions |= QwtPlotLayout::IgnoreFrames;
    }


    if ( d_data->discardFlags & DiscardLegend )
        layoutOptions |= QwtPlotLayout::IgnoreLegend;

    if ( d_data->discardFlags & DiscardTitle )
        layoutOptions |= QwtPlotLayout::IgnoreTitle;

    if ( d_data->discardFlags & DiscardFooter )
        layoutOptions |= QwtPlotLayout::IgnoreFooter;

    layout->activate( plot, layoutRect, layoutOptions );

    // canvas

    QwtScaleMap maps[QwtPlot::axisCnt];
    buildCanvasMaps( plot, layout->canvasRect(), maps );
    if ( updateCanvasMargins( plot, layout->canvasRect(), maps ) )
    {
        // recalculate maps and layout, when the margins
        // have been changed

        layout->activate( plot, layoutRect, layoutOptions );
        buildCanvasMaps( plot, layout->canvasRect(), maps );
    }

    // now start painting

    painter->save();
    painter->setWorldTransform( transform, true );

    renderCanvas( plot, painter, layout->canvasRect(), maps );

    if ( !( d_data->discardFlags & DiscardTitle )
        && ( !plot->titleLabel()->text().isEmpty() ) )
    {
        renderTitle( plot, painter, layout->titleRect() );
    }

    if ( !( d_data->discardFlags & DiscardFooter )
        && ( !plot->footerLabel()->text().isEmpty() ) )
    {
        renderFooter( plot, painter, layout->footerRect() );
    }

    if ( !( d_data->discardFlags & DiscardLegend )
        && plot->legend() && !plot->legend()->isEmpty() )
    {
        renderLegend( plot, painter, layout->legendRect() );
    }

    for ( int axisId = 0; axisId < QwtPlot::axisCnt; axisId++ )
    {
        QwtScaleWidget *scaleWidget = plot->axisWidget( axisId );
        if ( scaleWidget )
        {
            int baseDist = scaleWidget->margin();

            int startDist, endDist;
            scaleWidget->getBorderDistHint( startDist, endDist );

            renderScale( plot, painter, axisId, startDist, endDist,
                baseDist, layout->scaleRect( axisId ) );
        }
    }

    painter->restore();

    // restore all setting to their original attributes.
    for ( int axisId = 0; axisId < QwtPlot::axisCnt; axisId++ )
    {
        if ( d_data->layoutFlags & FrameWithScales )
        {
            QwtScaleWidget *scaleWidget = plot->axisWidget( axisId );
            if ( scaleWidget  )
                scaleWidget->setMargin( baseLineDists[axisId] );
        }

        layout->setCanvasMargin( canvasMargins[axisId] );
    }

    layout->invalidate();

}

/*!
  Render the title into a given rectangle.

  \param plot Plot widget
  \param painter Painter
  \param rect Bounding rectangle
*/
void QwtPlotRenderer::renderTitle( const QwtPlot *plot,
    QPainter *painter, const QRectF &rect ) const
{
    painter->setFont( qwtResolvedFont( plot->titleLabel() ) );

    const QColor color = plot->titleLabel()->palette().color(
            QPalette::Active, QPalette::Text );

    painter->setPen( color );
    plot->titleLabel()->text().draw( painter, rect );
}

/*!
  Render the footer into a given rectangle.

  \param plot Plot widget
  \param painter Painter
  \param rect Bounding rectangle
*/
void QwtPlotRenderer::renderFooter( const QwtPlot *plot,
    QPainter *painter, const QRectF &rect ) const
{
    painter->setFont( qwtResolvedFont( plot->footerLabel() ) );

    const QColor color = plot->footerLabel()->palette().color(
            QPalette::Active, QPalette::Text );

    painter->setPen( color );
    plot->footerLabel()->text().draw( painter, rect );
}

/*!
  Render the legend into a given rectangle.

  \param plot Plot widget
  \param painter Painter
  \param rect Bounding rectangle
*/
void QwtPlotRenderer::renderLegend( const QwtPlot *plot,
    QPainter *painter, const QRectF &rect ) const
{
    if ( plot->legend() )
    {
        bool fillBackground = !( d_data->discardFlags & DiscardBackground );
        plot->legend()->renderLegend( painter, rect, fillBackground );
    }
}

/*!
  \brief Paint a scale into a given rectangle.
  Paint the scale into a given rectangle.

  \param plot Plot widget
  \param painter Painter
  \param axisId Axis
  \param startDist Start border distance
  \param endDist End border distance
  \param baseDist Base distance
  \param rect Bounding rectangle
*/
void QwtPlotRenderer::renderScale( const QwtPlot *plot,
    QPainter *painter,
    int axisId, int startDist, int endDist, int baseDist,
    const QRectF &rect ) const
{
    if ( !plot->axisEnabled( axisId ) )
        return;

    const QwtScaleWidget *scaleWidget = plot->axisWidget( axisId );
    if ( scaleWidget->isColorBarEnabled()
        && scaleWidget->colorBarWidth() > 0 )
    {
        scaleWidget->drawColorBar( painter, scaleWidget->colorBarRect( rect ) );
        baseDist += scaleWidget->colorBarWidth() + scaleWidget->spacing();
    }

    painter->save();

    QwtScaleDraw::Alignment align;
    double x, y, w;

    switch ( axisId )
    {
        case QwtPlot::yLeft:
        {
            x = rect.right() - 1.0 - baseDist;
            y = rect.y() + startDist;
            w = rect.height() - startDist - endDist;
            align = QwtScaleDraw::LeftScale;
            break;
        }
        case QwtPlot::yRight:
        {
            x = rect.left() + baseDist;
            y = rect.y() + startDist;
            w = rect.height() - startDist - endDist;
            align = QwtScaleDraw::RightScale;
            break;
        }
        case QwtPlot::xTop:
        {
            x = rect.left() + startDist;
            y = rect.bottom() - 1.0 - baseDist;
            w = rect.width() - startDist - endDist;
            align = QwtScaleDraw::TopScale;
            break;
        }
        case QwtPlot::xBottom:
        {
            x = rect.left() + startDist;
            y = rect.top() + baseDist;
            w = rect.width() - startDist - endDist;
            align = QwtScaleDraw::BottomScale;
            break;
        }
        default:
            return;
    }

    scaleWidget->drawTitle( painter, align, rect );

    painter->setFont( qwtResolvedFont( scaleWidget ) );

    QwtScaleDraw *sd = const_cast<QwtScaleDraw *>( scaleWidget->scaleDraw() );
    const QPointF sdPos = sd->pos();
    const double sdLength = sd->length();

    sd->move( x, y );
    sd->setLength( w );

    QPalette palette = scaleWidget->palette();
    palette.setCurrentColorGroup( QPalette::Active );
    sd->draw( painter, palette );

    // reset previous values
    sd->move( sdPos );
    sd->setLength( sdLength );

    painter->restore();
}

/*!
  Render the canvas into a given rectangle.

  \param plot Plot widget
  \param painter Painter
  \param map Maps mapping between plot and paint device coordinates
  \param canvasRect Canvas rectangle
*/
void QwtPlotRenderer::renderCanvas( const QwtPlot *plot,
    QPainter *painter, const QRectF &canvasRect,
    const QwtScaleMap *map ) const
{
    const QWidget *canvas = plot->canvas();

    QRectF r = canvasRect.adjusted( 0.0, 0.0, -1.0, -1.0 );

    if ( d_data->layoutFlags & FrameWithScales )
    {
        painter->save();

        r.adjust( -1.0, -1.0, 1.0, 1.0 );
        painter->setPen( QPen( Qt::black ) );

        if ( !( d_data->discardFlags & DiscardCanvasBackground ) )
        {
            const QBrush bgBrush =
                canvas->palette().brush( plot->backgroundRole() );
            painter->setBrush( bgBrush );
        }

        QwtPainter::drawRect( painter, r );

        painter->restore();
        painter->save();

        painter->setClipRect( canvasRect );
        plot->drawItems( painter, canvasRect, map );

        painter->restore();
    }
    else if ( canvas->testAttribute( Qt::WA_StyledBackground ) )
    {
        QPainterPath clipPath;

        painter->save();

        if ( !( d_data->discardFlags & DiscardCanvasBackground ) )
        {
            QwtPainter::drawBackgound( painter, r, canvas );
            clipPath = qwtCanvasClip( canvas, canvasRect );
        }

        painter->restore();
        painter->save();

        if ( clipPath.isEmpty() )
            painter->setClipRect( canvasRect );
        else
            painter->setClipPath( clipPath );

        plot->drawItems( painter, canvasRect, map );

        painter->restore();
    }
    else
    {
        QPainterPath clipPath;

        int frameWidth = 0;

        if ( !( d_data->discardFlags & DiscardCanvasFrame ) )
        {
            const QVariant fw = canvas->property( "frameWidth" );
            if ( fw.type() == QVariant::Int )
                frameWidth = fw.toInt();

            clipPath = qwtCanvasClip( canvas, canvasRect );
        }

        QRectF innerRect = canvasRect.adjusted(
            frameWidth, frameWidth, -frameWidth, -frameWidth );

        painter->save();

        if ( clipPath.isEmpty() )
        {
            painter->setClipRect( innerRect );
        }
        else
        {
            painter->setClipPath( clipPath );
        }

        if ( !( d_data->discardFlags & DiscardCanvasBackground ) )
        {
            QwtPainter::drawBackgound( painter, innerRect, canvas );
        }

        plot->drawItems( painter, innerRect, map );

        painter->restore();

        if ( frameWidth > 0 )
        {
            painter->save();

            const int frameStyle =
                canvas->property( "frameShadow" ).toInt() |
                canvas->property( "frameShape" ).toInt();

            const QVariant borderRadius = canvas->property( "borderRadius" );
            if ( borderRadius.type() == QVariant::Double
                && borderRadius.toDouble() > 0.0 )
            {
                const double radius = borderRadius.toDouble();

                QwtPainter::drawRoundedFrame( painter, canvasRect,
                    radius, radius, canvas->palette(), frameWidth, frameStyle );
            }
            else
            {
                const int midLineWidth = canvas->property( "midLineWidth" ).toInt();

                QwtPainter::drawFrame( painter, canvasRect,
                    canvas->palette(), canvas->foregroundRole(),
                    frameWidth, midLineWidth, frameStyle );
            }
            painter->restore();
        }
    }
}

/*!
   Calculated the scale maps for rendering the canvas

   \param plot Plot widget
   \param canvasRect Target rectangle
   \param maps Scale maps to be calculated
*/
void QwtPlotRenderer::buildCanvasMaps( const QwtPlot *plot,
    const QRectF &canvasRect, QwtScaleMap maps[] ) const
{
    for ( int axisId = 0; axisId < QwtPlot::axisCnt; axisId++ )
    {
        maps[axisId].setTransformation(
            plot->axisScaleEngine( axisId )->transformation() );

        const QwtScaleDiv &scaleDiv = plot->axisScaleDiv( axisId );
        maps[axisId].setScaleInterval(
            scaleDiv.lowerBound(), scaleDiv.upperBound() );

        double from, to;
        if ( plot->axisEnabled( axisId ) )
        {
            const int sDist = plot->axisWidget( axisId )->startBorderDist();
            const int eDist = plot->axisWidget( axisId )->endBorderDist();
            const QRectF scaleRect = plot->plotLayout()->scaleRect( axisId );

            if ( axisId == QwtPlot::xTop || axisId == QwtPlot::xBottom )
            {
                from = scaleRect.left() + sDist;
                to = scaleRect.right() - eDist;
            }
            else
            {
                from = scaleRect.bottom() - eDist;
                to = scaleRect.top() + sDist;
            }
        }
        else
        {
            int margin = 0;
            if ( !plot->plotLayout()->alignCanvasToScale( axisId ) )
                margin = plot->plotLayout()->canvasMargin( axisId );

            if ( axisId == QwtPlot::yLeft || axisId == QwtPlot::yRight )
            {
                from = canvasRect.bottom() - margin;
                to = canvasRect.top() + margin;
            }
            else
            {
                from = canvasRect.left() + margin;
                to = canvasRect.right() - margin;
            }
        }
        maps[axisId].setPaintInterval( from, to );
    }
}

bool QwtPlotRenderer::updateCanvasMargins( QwtPlot *plot,
    const QRectF &canvasRect, const QwtScaleMap maps[] ) const
{
    double margins[QwtPlot::axisCnt];
    plot->getCanvasMarginsHint( maps, canvasRect,
        margins[QwtPlot::yLeft], margins[QwtPlot::xTop],
        margins[QwtPlot::yRight], margins[QwtPlot::xBottom] );

    bool marginsChanged = false;
    for ( int axisId = 0; axisId < QwtPlot::axisCnt; axisId++ )
    {
        if ( margins[axisId] >= 0.0 )
        {
            const int m = qCeil( margins[axisId] );
            plot->plotLayout()->setCanvasMargin( m, axisId);
            marginsChanged = true;
        }
    }

    return marginsChanged;
}

/*!
   \brief Execute a file dialog and render the plot to the selected file

   \param plot Plot widget
   \param documentName Default document name
   \param sizeMM Size for the document in millimeters.
   \param resolution Resolution in dots per Inch (dpi)

   \return True, when exporting was successful
   \sa renderDocument()
*/
bool QwtPlotRenderer::exportTo( QwtPlot *plot, const QString &documentName,
     const QSizeF &sizeMM, int resolution )
{
    if ( plot == NULL )
        return false;

    QString fileName = documentName;

    // What about translation

#ifndef QT_NO_FILEDIALOG
    const QList<QByteArray> imageFormats =
        QImageWriter::supportedImageFormats();

    QStringList filter;
#if QWT_FORMAT_PDF
    filter += QString( "PDF " ) + tr( "Documents" ) + " (*.pdf)";
#endif
#if QWT_FORMAT_SVG
    filter += QString( "SVG " ) + tr( "Documents" ) + " (*.svg)";
#endif
#if QWT_FORMAT_POSTSCRIPT
    filter += QString( "Postscript " ) + tr( "Documents" ) + " (*.ps)";
#endif

    if ( imageFormats.size() > 0 )
    {
        QString imageFilter( tr( "Images" ) );
        imageFilter += " (";
        for ( int i = 0; i < imageFormats.size(); i++ )
        {
            if ( i > 0 )
                imageFilter += " ";
            imageFilter += "*.";
            imageFilter += imageFormats[i];
        }
        imageFilter += ")";

        filter += imageFilter;
    }

    fileName = QFileDialog::getSaveFileName(
        NULL, tr( "Export File Name" ), fileName,
        filter.join( ";;" ), NULL, QFileDialog::DontConfirmOverwrite );
#endif
    if ( fileName.isEmpty() )
        return false;

    renderDocument( plot, fileName, sizeMM, resolution );

    return true;
}

#if QWT_MOC_INCLUDE
#include "moc_qwt_plot_renderer.cpp"
#endif
