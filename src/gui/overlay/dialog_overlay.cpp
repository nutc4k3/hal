#include "overlay/dialog_overlay.h"

#include <QVBoxLayout>

dialog_overlay::dialog_overlay(QWidget* parent) : overlay(parent),
    m_layout(new QVBoxLayout(this)),
    m_widget(nullptr)
{

}

void dialog_overlay::set_widget(QWidget* widget)
{
    if (m_widget)
    {
        m_widget->hide();
        m_widget->setParent(nullptr);
    }

    m_widget = widget;

    m_widget->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    m_layout->addWidget(widget, Qt::AlignCenter);
    m_layout->setAlignment(widget, Qt::AlignCenter);

    m_widget->show();
}
