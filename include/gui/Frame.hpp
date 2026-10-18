/**
 * Frame.hpp
 *
 * Container widget. Holds child widgets in its own local coordinate
 * space and can optionally show a draggable taskbar with a close button.
 *
 * Children are positioned relative to the frame's top-left corner.
 * Mouse coordinates are converted into local space before being passed
 * down, so a child's hit test needs no knowledge of where the frame is.
 */

#ifndef FRAME_HPP
#define FRAME_HPP

#include "WidgetManager.hpp"

class Button;
class TextBox;

class Frame : public Widget
{
private:
    WidgetManager m_widgets;

    // Taskbar parts are held directly rather than in m_widgets, so
    // drawing and event dispatch do not need a map lookup per widget to
    // work out which children are chrome.
    std::shared_ptr<TextBox> m_taskbar;
    std::shared_ptr<Button>  m_exit;

    // Borrowed. Must outlive this frame.
    const sf::Font* m_font{nullptr};

    bool m_show_taskbar{false};
    bool m_being_dragged{false};
    bool m_close_requested{false};

    float        m_taskbar_height{0.0f};
    sf::Vector2f m_drag_offset;

    void on_draw(sf::RenderTarget& target, sf::RenderStates states) const override;

    /*
     * Converts a point in the frame's parent space into its local space.
     */
    [[nodiscard]] sf::Vector2f to_local(sf::Vector2f parent_pos) const;

public:
    Frame(sf::Vector2f size, sf::Color color, const sf::Font& font);

    void begin_frame() override;
    void update(sf::Time delta_time) override;
    void handle_event(const sf::Event& event, sf::Vector2f mouse_pos) override;

    /*
     * Adds a title bar the frame can be dragged by, plus a close button
     * on its right. Safe to call more than once.
     */
    void add_taskbar(float height, sf::Color color, sf::Color outline_color,
                     float thickness, const std::string& title = "");

    [[nodiscard]] bool has_taskbar() const;

    void toggle_moveability();
    void set_moveable(bool moveable);

    void add_widget(const std::string& key, std::shared_ptr<Widget> widget);
    void remove_widget(const std::string& key);

    /*
     * Borrowed textures -- must outlive this frame. No-ops if
     * add_taskbar has not been called.
     */
    void add_taskbar_texture(const sf::Texture& texture, bool reset_rect = false);
    void add_taskbar_exit_texture(const sf::Texture& texture, bool reset_rect = false);

    /*
     * True for the frame in which the close button was clicked.
     */
    [[nodiscard]] bool close_requested() const;

    template<typename T = Widget>
    [[nodiscard]] std::shared_ptr<T> get_widget(const std::string& key) const;
};

template<typename T>
std::shared_ptr<T> Frame::get_widget(const std::string& key) const
{
    return m_widgets.get_widget<T>(key);
}

#endif
