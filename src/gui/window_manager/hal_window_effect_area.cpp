#include "window_manager/hal_window_effect_area.h"

#include "window_manager/hal_window_toolbar.h"
#include "window_manager/workspace.h"

#include <QKeyEvent>
#include <QResizeEvent>

hal_window_effect_area::hal_window_effect_area(QWidget* parent) : QFrame(parent),
    m_toolbar(new hal_window_toolbar(this)),
    m_workspace(new workspace(this))
{
    m_toolbar->setGeometry(0, 0, width(), 30);
    m_workspace->setGeometry(0, 30, width(), height() - 30);
}

void hal_window_effect_area::keyPressEvent(QKeyEvent* event)
{
    event->ignore();
}

void hal_window_effect_area::resizeEvent(QResizeEvent* event)
{
    m_toolbar->setGeometry(0, 0, event->size().width(), 30);
    m_workspace->setGeometry(0, 30, event->size().width(), event->size().height() - 30);

    QFrame::resizeEvent(event);
}
