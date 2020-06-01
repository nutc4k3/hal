#pragma once

#include <QFrame>

class QActionEvent;
class QHBoxLayout;

class window_toolbar final : public QFrame
{
    Q_OBJECT

public:
    explicit window_toolbar(QWidget* parent = nullptr);

    void add_widget(QWidget* widget);
    void add_spacer();
    void clear();

    void repolish();

protected:
    void actionEvent(QActionEvent* event) override;

private:
    QHBoxLayout* m_layout;
};
