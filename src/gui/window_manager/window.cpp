#include "gui/window_manager/window.h"

#include "gui/gui_globals.h"
#include "gui/overlay/overlay.h"
#include "gui/window_manager/window_effect_area.h"
#include "gui/window_manager/window_toolbar.h"
#include "gui/window_manager/workspace.h"

#include <QGraphicsBlurEffect>
#include <QResizeEvent>
#include <QStyle>

Window::Window(QWidget* parent) : QWidget(parent),
    m_toolbar(new window_toolbar(this)),
    m_workspace(new workspace(this)),
    m_effect_area(new window_effect_area(this)),
    m_overlay(nullptr),
    m_effect(nullptr)
{
    m_effect_area->setGeometry(0, 0, width(), height());
}

void Window::update_layout()
{
    QSize s = size();

    if (m_toolbar)
    {
        // TOOLBAR HEIGHT CONFIGURABLE ?
        m_toolbar->setGeometry(0, 0, s.width(), 30);
        m_workspace->setGeometry(0, 30, s.width(), s.height() - 30);
    }
    else
        m_workspace->setGeometry(0, 0, s.width(), s.height());
}

void Window::lock(overlay* const o, QGraphicsEffect* const e)
{
    assert(m_effect_area->isEnabled());

    m_effect_area->setEnabled(false);

    m_overlay = o;
    m_effect = e;

    m_overlay->setParent(this);
    m_overlay->show();

    m_effect_area->setGraphicsEffect(m_effect);
}

void Window::unlock()
{
    assert(!m_effect_area->isEnabled());

    m_effect_area->setEnabled(true);

    delete m_overlay;
    delete m_effect;

    m_overlay = nullptr;
    m_effect = nullptr;
}

void Window::standard_view()
{
    //m_workspace->show();
}

void Window::special_view(QWidget* widget)
{
    //m_workspace->hide();
    //m_inner_layout->addWidget(widget);
}

void Window::repolish()
{
    QStyle* s = style();

    s->unpolish(this);
    s->polish(this);

    //m_toolbar->repolish();
    //m_workspace->repolish();
    // REPOLISH CONTENT THROUGH CONTENT MANAGER

    //rearrange();
}

void Window::closeEvent(QCloseEvent* event)
{
    g_window_manager->handle_window_close_request(this);
    event->ignore();
}

void Window::resizeEvent(QResizeEvent* event)
{
    m_effect_area->resize(event->size());
    event->accept();
}
