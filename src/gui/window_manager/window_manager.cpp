#include "gui/window_manager/window_manager.h"

#include "gui/overlay/overlay.h"
#include "gui/plugin_management/plugin_schedule_widget.h"
#include "gui/settings/main_settings_widget.h"
#include "gui/style/style.h"
#include "gui/style/shared_properties_qss_adapter.h"
#include "gui/welcome_screen/welcome_screen.h"
#include "gui/window_manager/window.h"
#include "gui/window_manager/window_toolbar.h"

#include <QAction>
#include <QGraphicsBlurEffect>
#include <QShortcut>

#include <QDebug>

window_manager::window_manager(QObject* parent) : QObject(parent),
    m_main_window        (nullptr),
    m_action_open_file   (new QAction("Open File", this)),
    m_action_close_file  (new QAction("Close File", this)),
    m_action_save        (new QAction("Save", this)),
    m_action_schedule    (new QAction("Schedule", this)),
    m_action_run_schedule(new QAction("Run Schedule", this)),
    m_action_content     (new QAction("Content", this)),
    m_action_settings    (new QAction("Settings", this)),
    m_action_about       (new QAction("About", this)),
    m_welcome_screen     (new welcome_screen()),
    m_plugin_schedule_widget(new plugin_schedule_widget()),
    m_main_settings_widget(new main_settings_widget())
{
    // DEBUG CODE
    // CHECK IF SHORTCUTS WORK AS EXPECTED
    m_action_open_file->setShortcut(QKeySequence("Ctrl+O"));
    m_action_save->setShortcut(QKeySequence("Ctrl+S"));
    m_action_run_schedule->setShortcut(QKeySequence("Ctrl+Shift+R"));

    connect(m_action_open_file,    &QAction::triggered, this, &window_manager::handle_action_open);
    connect(m_action_close_file,   &QAction::triggered, this, &window_manager::handle_action_close);
    connect(m_action_save,         &QAction::triggered, this, &window_manager::handle_action_save);
    connect(m_action_schedule,     &QAction::triggered, this, &window_manager::handle_action_schedule);
    connect(m_action_run_schedule, &QAction::triggered, this, &window_manager::handle_action_run_schedule);
    connect(m_action_content,      &QAction::triggered, this, &window_manager::handle_action_content);
    connect(m_action_settings,     &QAction::triggered, this, &window_manager::handle_action_settings);
    connect(m_action_about,        &QAction::triggered, this, &window_manager::handle_action_about);

    repolish();

    // LOAD ALL LAYOUTS
    // RESTORE SELECTED LAYOUT OR USE DEFAULT LAYOUT

    // DEBUG CODE
    add_window();
    add_window();

    m_main_window->special_view(m_welcome_screen);

    // THIS WORKS, COMPARE TO HARDCODED EVENT LISTENER
    QShortcut* shortcut = new QShortcut(QKeySequence("F1"), m_windows.at(0));
    shortcut->setContext(Qt::ApplicationShortcut);
    connect(shortcut, &QShortcut::activated, this, &window_manager::handle_action_close);
}

void window_manager::add_window()
{
    Window* w = new Window(nullptr);
    m_windows.append(w);

    if (!m_main_window)
        set_main_window(w);

    w->show();
}

void window_manager::remove_window(Window* window)
{
    if (!window)
        return;

    if (m_windows.removeOne(window))
    {
        if (window == m_main_window)
        {
            if (!m_windows.empty())
                set_main_window(m_windows[0]);
            else
                m_main_window = nullptr;
        }
        window->deleteLater();
    }
}

void window_manager::set_main_window(Window* window)
{
    /*
    if (m_main_window)
        m_main_window->get_toolbar()->clear();
    // REMOVE CORRECT ACTIONS

    // MAYBE BETTER TO JUST HAVE 1 TOOLBAR, LESS WORK TO MANAGE

    // CHANGE TOOLBAR CONTENT DEPENDING ON APPLICATION STATE
    // ADD FANCY ANIMATION
    // WRITE OWN TOOLBAR CLASS BECAUSE QT CLASS SUCKS
    window_toolbar* t = window->get_toolbar();

    t->addAction(m_action_open_file);
    t->addAction(m_action_save);
    t->addAction(m_action_schedule);
    t->addAction(m_action_run_schedule);
    t->addAction(m_action_content);
    t->add_spacer();
    t->addAction(m_action_settings);

    m_main_window = window;
    */
}

void window_manager::lock_all()
{
    for (Window*& window : m_windows)
    {
        //DEBUG CODE
        overlay* o = new overlay();
        QGraphicsBlurEffect* e = new QGraphicsBlurEffect();

        e->setBlurHints(QGraphicsBlurEffect::QualityHint);

        connect(o, &overlay::clicked, this, &window_manager::handle_overlay_clicked);

        window->lock(o, e);
    }
}

void window_manager::unlock_all()
{
    for (Window*& window : m_windows)
        window->unlock();
}

void window_manager::handle_window_close_request(Window* window)
{
    Q_UNUSED(window);
    if (m_static_windows)
    {
        // ASK FOR CONFIRMATION / FORWARD TO WINDOW MANAGER WIDGET
    }
    else
    {
        // STORE CONTENT AND CLOSE WINDOW
    }
}

void window_manager::repolish()
{
    const shared_properties_qss_adapter* a = shared_properties_qss_adapter::instance();

    m_action_open_file   ->setIcon(style::get_styled_svg_icon(a->m_open_icon_style, a->m_open_icon_path));
    //m_action_close_file  ->setIcon(style::get_styled_svg_icon(a->m_close_icon_style, a->m_close_icon_path));
    m_action_save        ->setIcon(style::get_styled_svg_icon(a->m_save_icon_style, a->m_save_icon_path));
    m_action_schedule    ->setIcon(style::get_styled_svg_icon(a->m_schedule_icon_style, a->m_schedule_icon_path));
    m_action_run_schedule->setIcon(style::get_styled_svg_icon(a->m_run_icon_style, a->m_run_icon_path));
    m_action_content     ->setIcon(style::get_styled_svg_icon(a->m_content_icon_style, a->m_content_icon_path));
    m_action_settings    ->setIcon(style::get_styled_svg_icon(a->m_settings_icon_style, a->m_settings_icon_path));
    //m_action_about       ->setIcon(style::get_styled_svg_icon(a->m_about_icon_style, a->m_about_icon_path));

    // UPDATE ICONS IN TOOLBAR

    for (Window*& window : m_windows)
        window->repolish();
}

void window_manager::handle_overlay_clicked()
{
    unlock_all();
}

void window_manager::handle_action_open()
{
    qDebug() << "handle action open called";
    lock_all(); // DEBUG CODE
}

void window_manager::handle_action_close()
{
    qDebug() << "handle action close called";
}

void window_manager::handle_action_save()
{

}

void window_manager::handle_action_schedule()
{

}

void window_manager::handle_action_run_schedule()
{

}

void window_manager::handle_action_content()
{

}

void window_manager::handle_action_settings()
{

}

void window_manager::handle_action_about()
{

}
