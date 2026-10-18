/**
 * Button.hpp
 *
 * Clickable widget with hover and press states.
 *
 * Click detection is edge-based: a click is a press and a release that
 * both land inside the button, and it fires the callback exactly once.
 * The old version polled sf::Mouse::isButtonPressed, so it reported
 * "pressed" on every frame the mouse was held and fired its callback
 * once per queued event per frame.
 */

#ifndef BUTTON_HPP
#define BUTTON_HPP

#include "TextWidget.hpp"

class Button : public TextWidget
{
private:
    sf::Color m_hover_color;
    sf::Color m_press_color;

    bool m_hovered{false};
    bool m_held{false};       // mouse went down on us and is still down
    bool m_clicked{false};    // completed click this frame

    void on_draw(sf::RenderTarget& target, sf::RenderStates states) const override;
    void create_color_variants();

public:
    Button(sf::Vector2f size, sf::Color color);

    void begin_frame() override;
    void update(sf::Time delta_time) override;
    void handle_event(const sf::Event& event, sf::Vector2f mouse_pos) override;

    /*
     * How much darker the hover and press tints are than the base
     * colour. Defaults to 40 and 80.
     */
    void set_state_tints(int hover_amount, int press_amount);

    [[nodiscard]] bool is_hovered() const;

    /*
     * True while the mouse is held down on the button.
     */
    [[nodiscard]] bool is_pressed() const;

    /*
     * True for the single frame in which a click completed. Reset by
     * begin_frame(), so read it after events and before the next frame.
     */
    [[nodiscard]] bool was_clicked() const;
};

#endif
