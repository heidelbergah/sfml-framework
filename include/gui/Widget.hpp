/**
 * Widget.hpp
 *
 * Base class for every GUI element.
 *
 * Widgets draw directly into the target through an accumulated
 * transform. They no longer each own an sf::RenderTexture -- that meant
 * an FBO bind, a clear, several draws and a texture rebind per widget
 * per frame, whether or not anything had changed, and it made draw()
 * mutate state so the same widget could not be drawn to two targets.
 *
 * Textures and fonts are borrowed, not owned. Load them through
 * Assets::textures() / Assets::fonts() and pass the reference in; the
 * cache outlives every scene, so the pointer stays valid.
 */

#ifndef WIDGET_HPP
#define WIDGET_HPP

#include "../Includes.hpp"

enum class Alignment
{
    CENTER,
    LEFT,
    RIGHT
};

class Widget : public sf::Drawable
{
protected:
    sf::RectangleShape m_background;
    sf::RectangleShape m_outline;
    sf::Color          m_color{sf::Color::White};
    sf::Color          m_outline_color{sf::Color::Transparent};

    Interpolated<sf::Vector2f> m_pos;
    sf::Vector2f               m_size;

    // Borrowed, not owned. Owned by an AssetCache that outlives us.
    const sf::Texture* m_background_texture{nullptr};

    unsigned int m_z_value{0};      // higher draws later, on top
    bool         m_visible{true};
    bool         m_enabled{true};

    std::optional<std::function<void()>> m_callback;

    /*
     * Implemented by each widget. The transform for this widget is
     * already baked into `states`, so draw children at local
     * coordinates starting from (0, 0).
     */
    virtual void on_draw(sf::RenderTarget& target, sf::RenderStates states) const = 0;

    /*
     * Applies this widget's transform and hands off to on_draw.
     */
    void draw(sf::RenderTarget& target, sf::RenderStates states) const final;

public:
    explicit Widget(sf::Vector2f size);
    ~Widget() override = default;

    /*
     * Clears per-frame flags such as "was clicked this frame". Call
     * once at the top of the frame, before feeding in events.
     */
    virtual void begin_frame() {}

    virtual void update(sf::Time delta_time) { (void)delta_time; }

    /*
     * `mouse_pos` is in this widget's parent coordinate space. A
     * container converts into its own local space before passing down.
     */
    virtual void handle_event(const sf::Event& event, sf::Vector2f mouse_pos)
    {
        (void)event;
        (void)mouse_pos;
    }

    void set_position(sf::Vector2f pos, bool instant = false);
    void set_size(sf::Vector2f size);
    void set_color(sf::Color color);
    void set_outline(sf::Color color, float thickness);
    void set_z_value(unsigned int z);
    void set_visible(bool visible);
    void set_enabled(bool enabled);

    void set_callback(std::function<void()> callback);

    void set_transition(TransitionFunction transition);
    void set_transition_duration(float duration);

    /*
     * Borrows the texture -- it must outlive this widget. Pass one from
     * Assets::textures(), never a local.
     */
    void set_background_texture(const sf::Texture& texture, bool reset_rect = false);
    void clear_background_texture();

    void move(sf::Vector2f offset);

    /*
     * Transform for this widget, translation snapped to whole pixels so
     * pixel art does not shimmer between frames.
     */
    [[nodiscard]] sf::Transform get_transform() const;

    [[nodiscard]] sf::Vector2f get_position() const;
    [[nodiscard]] sf::Vector2f get_target_position() const;
    [[nodiscard]] sf::Vector2f get_size() const;
    [[nodiscard]] sf::Color    get_color() const;

    /*
     * Bounds in the widget's own parent space.
     */
    [[nodiscard]] sf::FloatRect get_global_bounds() const;

    /*
     * Hit test. `point` must be in the same space as get_global_bounds.
     */
    [[nodiscard]] bool contains(sf::Vector2f point) const;

    [[nodiscard]] TransitionFunction get_transition() const;
    [[nodiscard]] unsigned int       get_z_value() const;
    [[nodiscard]] bool               is_visible() const;
    [[nodiscard]] bool               is_enabled() const;
};

#endif
