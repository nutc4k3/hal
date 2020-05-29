#include "gui/window_manager/window_effect_area.h"

#include <QKeyEvent>

window_effect_area::window_effect_area(QWidget* parent) : QWidget(parent)
{
}

void window_effect_area::keyPressEvent(QKeyEvent* event)
{
    event->ignore();
}
