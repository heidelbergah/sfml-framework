/**
 * Scene.hpp
 *
 * One screen of the game: a main menu, the game itself, a cutscene,
 * a shop. SceneManager owns the window and the loop; a Scene only
 * decides what happens inside it.
 *
 * Scenes no longer hold raw pointers to the window, render texture and
 * sprite. The manager passes the render target into render() and the
 * already-mapped mouse position into handle_event(), so a scene cannot
 * be constructed in a state where those pointers are uninitialised.
 */

#ifndef SCENE_HPP
#define SCENE_HPP

#include "../Includes.hpp"

class SceneManager;

class Scene
{
protected:
    // Set by SceneManager when the scene is added. Use it to request
    // scene changes, or publish an EventType instead.
    SceneManager* m_manager{nullptr};

public:
    virtual ~Scene() = default;

    /*
     * Called each time the scene becomes active. A scene can be entered
     * more than once, so reset per-run state here rather than in the
     * constructor.
     */
    virtual void on_enter() {}

    /*
     * Called when the scene stops being active. Release anything that
     * should not persist while another scene runs.
     */
    virtual void on_exit() {}

    /*
     * Called after each fixed update, once that update has seen the
     * input gathered since the previous one. Clear per-frame flags here,
     * typically by forwarding to your WidgetManager's begin_frame(),
     * which resets things like "this button was clicked this frame".
     *
     * So a flag set by an event stays true for exactly one update(),
     * however many times the loop spins between updates.
     */
    virtual void begin_frame() {}

    /*
     * One window event. `mouse_pos` is in base-resolution coordinates
     * (already divided by SCALE), so it lines up with widget positions.
     * Map it through your own camera view for world coordinates.
     */
    virtual void handle_event(const sf::Event& event, sf::Vector2f mouse_pos)
    {
        (void)event;
        (void)mouse_pos;
    }

    /*
     * While the game is paused this is called with zero, so gameplay
     * written the obvious way, accumulating dt, integrating positions,
     * freezes without needing to check a pause flag. Under slow motion it
     * shrinks proportionally.
     */
    virtual void update(sf::Time delta_time) { (void)delta_time; }

    /*
     * Fixed timestep, UNSCALED. Always the real frame duration, even
     * while paused. Use for anything that must keep running regardless:
     * a pause menu, a loading spinner, UI transitions.
     */
    virtual void update_realtime(sf::Time delta_time) { (void)delta_time; }

    /*
     * Draw into the low-resolution target. The manager clears it,
     * scales it up and presents it.
     */
    virtual void render(sf::RenderTarget& target) { (void)target; }

    /*
     * Background colour for this scene's frame.
     */
    [[nodiscard]] virtual sf::Color get_clear_color() const
    {
        return sf::Color(20, 20, 28);
    }

    friend class SceneManager;
};

#endif

