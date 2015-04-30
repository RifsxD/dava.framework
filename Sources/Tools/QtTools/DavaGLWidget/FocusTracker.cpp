#include "FocusTracker.h"

#include "DavaGLWidget.h"

#include <QApplication>
#include <QLineEdit>
#include <QSpinBox>
#include <QDebug>


FocusTracker::FocusTracker( DavaGLWidget* _glWidget )
    : QObject( _glWidget )
    , glWidget( _glWidget )
    , glWindow( _glWidget->GetGLWindow() )
    , isFocused( false )
{
    glWindow->installEventFilter( this );
}

FocusTracker::~FocusTracker()
{}

bool FocusTracker::eventFilter( QObject* watched, QEvent* event )
{
    if ( watched == glWindow )
    {
        switch ( event->type() )
        {
        case QEvent::MouseButtonPress:
            OnClick();
            break;
        case QEvent::FocusIn:
            OnFocusIn();
            break;
        case QEvent::FocusOut:
            OnFocusOut();
            break;

        default:
            break;
        }
    }

    return QObject::eventFilter( watched, event );
}

void FocusTracker::OnClick()
{
    if ( !isFocused )
    {
        glWindow->requestActivate();
    }
}

void FocusTracker::OnFocusIn()
{
    qDebug() << __FUNCTION__;

    isFocused = true;
    emit focusIn();
}

void FocusTracker::OnFocusOut()
{
    qDebug() << __FUNCTION__;

    isFocused = false;
    emit focusOut();
}
