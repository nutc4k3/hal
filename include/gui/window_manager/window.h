#pragma once

#include <QWidget>

class overlay;
class window_effect_area;
class window_toolbar;
class workspace;

class Window : public QWidget
{
    Q_OBJECT

public:
    explicit Window(QWidget* parent = nullptr);

    void update_layout();

    void lock(overlay* const o, QGraphicsEffect* const e);
    void unlock();

    void standard_view();
    void special_view(QWidget* widget);

    void repolish();

    window_toolbar* m_toolbar;
    workspace* const m_workspace;

protected:
    void closeEvent(QCloseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    window_effect_area* m_effect_area;
    overlay* m_overlay;
    QGraphicsEffect* m_effect;
};
