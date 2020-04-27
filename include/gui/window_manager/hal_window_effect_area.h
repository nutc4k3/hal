#ifndef HAL_WINDOW_EFFECT_AREA_H
#define HAL_WINDOW_EFFECT_AREA_H

#include <QFrame>

class hal_window_toolbar;
class workspace;

class QKeyEvent;

class hal_window_effect_area : public QFrame
{
    Q_OBJECT

public:
    explicit hal_window_effect_area(QWidget* parent = nullptr);

    hal_window_toolbar* m_toolbar;
    workspace* m_workspace;

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
};

#endif // HAL_WINDOW_EFFECT_AREA_H
