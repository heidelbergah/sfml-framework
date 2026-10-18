/**
 * Slider.hpp
 *
 * Horizontal slider with a draggable thumb.
 *
 * The authoritative state is a normalised value in [0, 1]; the thumb's
 * pixel position is derived from it. Previously the thumb's position
 * was the state, which meant get_percentage() had to reverse the
 * geometry and there was no way to set the value programmatically.
 */

#ifndef SLIDER_HPP
#define SLIDER_HPP

#include "Widget.hpp"

class Slider : public Widget
{
private:
    sf::RectangleShape m_thumb;
    sf::RectangleShape m_body;

    sf::Color m_thumb_color;
    sf::Color m_body_color{sf::Color::White};

    float m_value{0.5f};          // normalised, [0, 1]
    bool  m_being_dragged{false};
    bool  m_value_changed{false}; // changed this frame

    const sf::Texture* m_thumb_texture{nullptr};
    const sf::Texture* m_body_texture{nullptr};

    void on_draw(sf::RenderTarget& target, sf::RenderStates states) const override;

    /*
     * Moves the thumb to match m_value.
     */
    void sync_thumb_position();

    /*
     * Converts a local x coordinate into a normalised value.
     */
    [[nodiscard]] float value_from_local_x(float local_x) const;

public:
    Slider(sf::Vector2f size, sf::Color color);

    void begin_frame() override;
    void handle_event(const sf::Event& event, sf::Vector2f mouse_pos) override;

    /*
     * Normalised position of the thumb, 0 at the left, 1 at the right.
     */
    [[nodiscard]] float get_percentage() const;

    /*
     * Maps the slider onto an arbitrary range.
     */
    [[nodiscard]] float get_value(float min, float max) const;

    void set_percentage(float percentage);

    /*
     * True for the frame in which the value changed.
     */
    [[nodiscard]] bool value_changed() const;
    [[nodiscard]] bool is_being_dragged() const;

    void set_body_color(sf::Color color);
    void set_thumb_color(sf::Color color);

    /*
     * Borrowed textures -- must outlive this widget.
     */
    void set_thumb_texture(const sf::Texture& texture, bool reset_rect = false);
    void set_body_texture(const sf::Texture& texture, bool reset_rect = false);
};

#endif
