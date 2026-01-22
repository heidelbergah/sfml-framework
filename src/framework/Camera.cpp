#include "../../include/framework/Camera.hpp"

Camera::Camera(sf::Vector2f size, sf::Vector2f center)
{
    m_size = size;
    m_pos.set_instant(center);
    m_actual_pos = m_pos;

    m_view.setSize(m_size);
    m_view.setCenter(m_pos);
}

void Camera::move(sf::Vector2f displacement)
{
    float new_x = m_pos.get_value().x + displacement.x;
    float new_y = m_pos.get_value().y + displacement.y;
    m_pos = sf::Vector2f(new_x, new_y);
    m_actual_pos = m_pos;
}

void Camera::move(sf::Vector2f displacement, bool shake_move)
{
    float new_x = m_pos.get_value().x + displacement.x;
    float new_y = m_pos.get_value().y + displacement.y;
    m_pos = sf::Vector2f(new_x, new_y);
}

void Camera::shake(int intensity)
{
    float angle = rand() % 360;

    angle = sf::degrees(angle).asRadians();

    float new_x = cos(angle) * intensity;
    float new_y = -sin(angle) * intensity;

    move(sf::Vector2f(new_x, new_y), true);
}

void Camera::shake(int intensity, float degrees)
{
    float angle = sf::degrees(degrees).asRadians();

    float new_x = cos(angle) * intensity;
    float new_y = -sin(angle) * intensity;

    move(sf::Vector2f(new_x, new_y), true);
}

void Camera::update()
{
    if(m_pos.is_transition_over())
        m_pos = m_actual_pos;
    m_view.setCenter(m_pos);
}

void Camera::set_transition(TransitionFunction transFunc)
{
    m_pos.set_transition(transFunc);
}

void Camera::set_transition_duration(float duration)
{
    m_pos.set_duration(duration);
}

sf::View& Camera::get_view()
{
    return m_view;
}

