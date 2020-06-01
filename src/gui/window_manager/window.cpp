#include "gui/window_manager/window.h"

#include "gui/gui_globals.h"
#include "gui/overlay/overlay.h"
#include "gui/window_manager/window_effect_layer.h"
#include "gui/window_manager/window_toolbar.h"
#include "gui/window_manager/workspace.h"

#include <QGraphicsBlurEffect>
#include <QResizeEvent>
#include <QStyle>

Window::Window(QWidget* parent) : QWidget(parent),
    m_toolbar(new window_toolbar(this)),
    m_workspace(new workspace(this)),
    m_effect_layer(new Window_effect_layer(this)),
    m_active_widget(nullptr),
    m_overlay(nullptr),
    m_effect(nullptr)
{
    m_effect_layer->setGeometry(0, 0, width(), height());
}

void Window::update_layout()
{
    QSize s = size();

    if (m_toolbar)
    {
        const int toolbar_height = 30; // CONFIGURABLE ???

        m_toolbar->setGeometry(0, 0, s.width(), toolbar_height);
        m_active_widget->setGeometry(0, toolbar_height, s.width(), s.height() - toolbar_height);
    }
    else
        m_active_widget->setGeometry(0, 0, s.width(), s.height());
}

void Window::lock(overlay* const o, QGraphicsEffect* const e)
{
    assert(m_effect_layer->isEnabled());

    m_effect_layer->setEnabled(false);

    m_overlay = o;
    m_effect = e;

    m_overlay->setParent(this);
    m_overlay->show();

    m_effect_layer->setGraphicsEffect(m_effect);
}

void Window::unlock()
{
    assert(!m_effect_layer->isEnabled());

    m_effect_layer->setEnabled(true);

    delete m_overlay;
    delete m_effect;

    m_overlay = nullptr;
    m_effect = nullptr;
}

void Window::set_active_widget(QWidget* widget)
{
    m_active_widget->hide();
    m_active_widget = widget;
    m_active_widget->setParent(m_effect_layer);
    update_layout();
}

void Window::repolish()
{
    QStyle* s = style();

    s->unpolish(this);
    s->polish(this);

    //m_toolbar->repolish();
    //m_workspace->repolish();
    // REPOLISH CONTENT THROUGH CONTENT MANAGER
}

void Window::closeEvent(QCloseEvent* event)
{
    g_window_manager->handle_window_close_request(this);
    event->ignore();
}

void Window::resizeEvent(QResizeEvent* event)
{
    m_effect_layer->resize(event->size());
    event->accept();
}
