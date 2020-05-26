#ifndef DIALOG_OVERLAY_H
#define DIALOG_OVERLAY_H

#include "overlay/overlay.h"

class QVBoxLayout;

class dialog_overlay : public overlay
{
    Q_OBJECT

public:
    explicit dialog_overlay(QWidget* parent = nullptr);

    void set_widget(QWidget* widget);

private:
    QVBoxLayout* m_layout;
    QWidget* m_widget;
};

#endif // DIALOG_OVERLAY_H
