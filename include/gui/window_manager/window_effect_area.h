#pragma once

#include <QWidget>

class QKeyEvent;

class window_effect_area final : public QWidget
{
    Q_OBJECT

public:
    explicit window_effect_area(QWidget* parent = nullptr);

protected:
    void keyPressEvent(QKeyEvent* event) override;
};
