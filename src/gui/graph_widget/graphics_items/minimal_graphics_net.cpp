#include "graph_widget/graphics_items/minimal_graphics_net.h"

#include <QPainter>
#include <QPen>
#include <QPointF>
#include <QStyleOptionGraphicsItem>

minimal_graphics_net::minimal_graphics_net(std::shared_ptr<net> n) : graphics_net(n)
{
}

void minimal_graphics_net::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
    Q_UNUSED(widget);

    s_pen.setColor((option->state & QStyle::State_Selected) ? s_selection_color : m_color);
    painter->setPen(s_pen);

    //    if (g_selection_relay.m_focus_type == selection_relay::item_type::net)
    //        if (g_selection_relay.m_focus_net_id == m_id)
    //        {
    //            QPen pen(s_selection_color, 1, Qt::DashLine);
    //            pen.setJoinStyle(Qt::MiterJoin);
    //            //pen.setCosmetic(true);
    //            painter->setPen(pen);
    //        }

    painter->drawLines(m_lines);

#ifdef HAL_DEBUG_GUI_GRAPHICS
    s_pen.setColor(Qt::green);
    const bool cosmetic = s_pen.isCosmetic();
    s_pen.setCosmetic(true);
    painter->setPen(s_pen);
    painter->drawPath(m_shape);
    s_pen.setCosmetic(cosmetic);
#endif
}

void minimal_graphics_net::finalize()
{
    QPainterPathStroker stroker;
    stroker.setWidth(s_stroke_width);
    m_rect  = m_path.boundingRect();
    m_shape = stroker.createStroke(m_path);
}

void minimal_graphics_net::line_to_x(const qreal scene_x)
{
    if (scene_x == current_scene_position().x())
        return;

    QPointF mapped_point = mapFromScene(scene_x, current_scene_position().y());
    m_lines.append(QLineF(m_path.currentPosition(), mapped_point));
    m_path.lineTo(mapped_point);
}

void minimal_graphics_net::line_to_y(const qreal scene_y)
{
    if (scene_y == current_scene_position().y())
        return;

    QPointF mapped_point = mapFromScene(current_scene_position().x(), scene_y);
    m_lines.append(QLineF(m_path.currentPosition(), mapped_point));
    m_path.lineTo(mapped_point);
}

void minimal_graphics_net::move_pen_to(const QPointF& scene_position)
{
    if (scene_position == current_scene_position())
        return;

    QPointF mapped_point = mapFromScene(scene_position);
    m_path.moveTo(mapped_point);
}

QPointF minimal_graphics_net::current_scene_position() const
{
    return mapToScene(m_path.currentPosition());
}