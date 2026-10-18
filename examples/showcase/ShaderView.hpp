/**
 * ShaderView.hpp  -  Showcase
 *
 * A widget that owns a small render texture and lets an Effect draw
 * into it every frame. Put one inside a Frame and you have a window
 * running its own shader scene.
 *
 * Rendering into a texture of its own is what keeps each window self
 * contained: an effect cannot draw outside its rectangle (Frame does
 * not clip children), and several can run side by side.
 *
 * Input arrives the ordinary widget way, through the Frame's
 * handle_event, so an effect only ever sees the mouse when its window
 * is the one being routed to.
 */

#ifndef SHOWCASE_SHADER_VIEW_HPP
#define SHOWCASE_SHADER_VIEW_HPP

#include "../../include/framework/ShaderManager.hpp"
#include "../../include/gui/Frame.hpp"
#include "../../include/gui/Widget.hpp"

namespace showcase
{

/*
 * The mouse, as one effect sees it. Coordinates are local to the view,
 * in view pixels, with (0, 0) at the top left.
 */
struct Pointer
{
    sf::Vector2f pos;
    bool inside{false};    // over this view, and this view's window has the mouse
    bool down{false};      // left button held after pressing inside
    bool pressed{false};   // left button went down inside since the last update
};

class Effect
{
public:
    virtual ~Effect() = default;

    [[nodiscard]] virtual std::string  title() const = 0;
    [[nodiscard]] virtual sf::Vector2u size() const = 0;

    /*
     * Loads shaders and allocates targets. Return false if anything is
     * missing; the view then shows a short message instead.
     */
    virtual bool load(ShaderManager& shaders) = 0;

    virtual void update(float dt, const Pointer& pointer) = 0;

    /*
     * Draw the effect. `target` is size() pixels and already cleared.
     */
    virtual void render(sf::RenderTarget& target) = 0;

    /*
     * Optional controls below the view. The window is sized from
     * controls_height(), then build_controls() adds them into it with
     * `origin` at their top-left.
     */
    [[nodiscard]] virtual float controls_height() const { return 0.0f; }

    virtual void build_controls(Frame& frame, sf::Vector2f origin)
    {
        (void)frame;
        (void)origin;
    }

    virtual void on_open()  {}
    virtual void on_close() {}
};

class ShaderView : public Widget
{
private:
    Effect*           m_effect{nullptr};   // owned by the scene's window
    sf::RenderTexture m_target;
    Pointer           m_pointer;
    bool              m_ready{false};
    std::string       m_failure;

    void on_draw(sf::RenderTarget& target, sf::RenderStates states) const override;

public:
    ShaderView(Effect& effect, ShaderManager& shaders);

    void begin_frame() override;
    void update(sf::Time delta_time) override;
    void handle_event(const sf::Event& event, sf::Vector2f mouse_pos) override;
};

/*
 * Draws a quad covering `size` pixels with texture coordinates running
 * 0 to 1, so a fragment shader can read gl_TexCoord[0] as "where am I
 * in this rectangle" without any texture bound.
 */
void draw_quad(sf::RenderTarget& target, sf::Vector2u size, const sf::Shader* shader);

/*
 * Sets c0..c3, the four-colour ramp every effect quantises to.
 */
void set_ramp(sf::Shader& shader, sf::Color c0, sf::Color c1, sf::Color c2, sf::Color c3);

}

#endif
