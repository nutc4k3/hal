#include "window_manager/hal_window.h"

#include "gui/gui_globals.h"
#include "gui/overlay/overlay.h"
#include "gui/window_manager/hal_window_effect_area.h"

#include <QAction>
#include <QGraphicsBlurEffect>
#include <QResizeEvent>
#include <QStyle>

hal_window::hal_window(QWidget* parent) : QFrame(parent),
    m_effect_area(new hal_window_effect_area(this)),
    m_overlay(nullptr),
    m_effect(nullptr)
{
    m_effect_area->setGeometry(0, 0, width(), height());
}

void hal_window::lock()
{
    if (!m_overlay)
    {
        m_effect_area->setEnabled(false);

        m_overlay = new overlay(this); // DEBUG CODE, USE FACTORY AND STYLESHEETS ?
        m_effect = new QGraphicsBlurEffect();

        //m_effect->setBlurHints(QGraphicsBlurEffect::QualityHint);

        m_effect_area->setGraphicsEffect(m_effect);

        m_overlay->setParent(this);
        m_overlay->show();
        connect(m_overlay, &overlay::clicked, g_window_manager, &window_manager::handle_overlay_clicked);
    }
}

void hal_window::unlock()
{
    if (!m_effect_area->isEnabled())
    {
        m_effect_area->setEnabled(true);

        delete m_effect;
        delete m_overlay;

        m_effect = nullptr;
        m_overlay = nullptr;
    }
}

void hal_window::standard_view()
{
    //m_workspace->show();
}

void hal_window::special_view(QWidget* widget)
{
    //m_workspace->hide();
    //m_inner_layout->addWidget(widget);
}

void hal_window::repolish()
{
    QStyle* s = style();

    s->unpolish(this);
    s->polish(this);

    //m_toolbar->repolish();
    //m_workspace->repolish();
    // REPOLISH CONTENT THROUGH CONTENT MANAGER

    //rearrange();
}

hal_window_toolbar* hal_window::get_toolbar()
{
    return m_effect_area->m_toolbar;
}

overlay* hal_window::get_overlay()
{
    return m_overlay;
}

void hal_window::closeEvent(QCloseEvent* event)
{
    g_window_manager->handle_window_close_request(this);
    event->ignore();
}

void hal_window::resizeEvent(QResizeEvent* event)
{
    m_effect_area->resize(event->size());
    event->accept();
}
