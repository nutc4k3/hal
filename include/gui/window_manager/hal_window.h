#ifndef HAL_WINDOW_H
#define HAL_WINDOW_H

#include <QFrame>

class hal_window_effect_area;
class hal_window_toolbar;
class overlay;

class QAction;

class hal_window : public QFrame
{
    Q_OBJECT

public:
    explicit hal_window(QWidget* parent = nullptr);

    void lock();
    void unlock();

    void standard_view();
    void special_view(QWidget* widget);

    void repolish();

    hal_window_toolbar* get_toolbar();
    overlay* get_overlay();

protected:
    //void paintEvent(QPaintEvent* event) Q_DECL_OVERRIDE;
    //bool event(QEvent* event) Q_DECL_OVERRIDE;
    void closeEvent(QCloseEvent* event) override;
//    void changeEvent(QEvent* event) Q_DECL_OVERRIDE;
    void resizeEvent(QResizeEvent* event) override;

private:
    hal_window_effect_area* m_effect_area;
    overlay* m_overlay;
    QGraphicsEffect* m_effect;
};

#endif // HAL_WINDOW_H
