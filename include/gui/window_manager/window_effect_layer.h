#pragma once

#include <QWidget>

class QKeyEvent;

class Window_effect_layer final : public QWidget
{
    Q_OBJECT

public:
    explicit Window_effect_layer(QWidget* parent = nullptr);

protected:
    void keyPressEvent(QKeyEvent* event) override;
};
