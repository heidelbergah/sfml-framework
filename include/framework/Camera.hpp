#ifndef CAMERA_HPP
#define CAMERA_HPP

#include "../Includes.hpp"

class Camera
{
private:
    sf::View m_view;
    Interpolated<sf::Vector2f> m_pos;
    sf::Vector2f m_actual_pos;  // When shaking, we need to keep track of our original position
    sf::Vector2f m_size;

    void move(sf::Vector2f displacement, bool shake_move);  // ONLY called by shake
public:
    Camera(sf::Vector2f size, sf::Vector2f center);

    void move(sf::Vector2f displacement);

    void shake(int intensity);  // Random shaking
    void shake(int intensity, float degrees);  // Defined shaking

    void update();

    void set_transition(TransitionFunction transFunc);

    void set_transition_duration(float duration);

    sf::View& get_view();

    // Gets the position of the camera as if its origin was in the top left corner
    sf::Vector2f get_position();

    // Gets the center of the camera
    sf::Vector2f get_center() const;
};

#endif
