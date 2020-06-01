#pragma once

#include <QFrame>

class workspace final : public QFrame
{
    Q_OBJECT

public:
    explicit workspace(QWidget* parent = nullptr);

    void repolish();
};
