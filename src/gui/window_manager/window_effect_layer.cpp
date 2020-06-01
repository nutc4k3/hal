#include "gui/window_manager/window_effect_layer.h"

#include <QKeyEvent>

Window_effect_layer::Window_effect_layer(QWidget* parent) : QWidget(parent)
{
}

void Window_effect_layer::keyPressEvent(QKeyEvent* event)
{
    event->ignore();
}
