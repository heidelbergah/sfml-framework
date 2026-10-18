/**
 * Camera.hpp
 *
 * View wrapper with tweened movement and stacking screen shake.
 *
 * Position and shake are kept separate. The old version pushed shake
 * offsets into the same Interpolated value it used for movement and
 * kept a backup to restore from, which meant a second shake replaced
 * the first instead of stacking, and update() re-assigned the position
 * every frame once the tween finished, restarting it continuously.
 *
 * Shake here is modelled as "trauma": a 0-1 value that sources add to
 * and that decays on its own. Offset scales with trauma squared, which
 * makes a big hit read as violent and a small one as a nudge. Because
 * trauma accumulates, overlapping sources combine instead of
 * overwriting -- explosions during machine-gun fire feel right.
 *
 * The offset is sampled from smooth value noise rather than fresh
 * randomness per frame, so the camera sweeps rather than vibrating.
 */

#ifndef CAMERA_HPP
#define CAMERA_HPP

#include "../Includes.hpp"

class Camera
{
private:
    sf::View m_view;

    Interpolated<sf::Vector2f> m_center;     // where the camera wants to be
    sf::Vector2f               m_size;

    float m_trauma{0.0f};                    // impulses; decays on its own
    float m_sustained{0.0f};                 // held floor while something rumbles
    float m_sustained_request{0.0f};         // strongest request this frame
    float m_sustained_decay{5.0f};           // how fast the floor lets go
    float m_trauma_decay{1.5f};              // trauma units per second
    float m_max_offset{24.0f};               // pixels at full trauma
    float m_max_roll{4.0f};                  // degrees at full trauma
    bool  m_roll_enabled{false};             // see set_shake_roll_enabled
    bool  m_pixel_snap{true};                // see set_pixel_snap
    float m_shake_frequency{22.0f};          // noise samples per second
    float m_shake_time{0.0f};

    // Independent noise channels, so x, y and roll are uncorrelated.
    float m_noise_seed_x{0.0f};
    float m_noise_seed_y{0.0f};
    float m_noise_seed_r{0.0f};

    sf::Vector2f m_shake_offset;
    sf::Angle    m_shake_roll;

    // Optional world bounds to keep the view inside.
    std::optional<sf::FloatRect> m_bounds;

    [[nodiscard]] sf::Vector2f clamp_to_bounds(sf::Vector2f center) const;

public:
    Camera(sf::Vector2f size, sf::Vector2f center);

    /*
     * Advance the shake simulation and apply the result to the view.
     * Call once per frame.
     */
    void update(sf::Time delta_time);

    void move(sf::Vector2f displacement);
    void set_center(sf::Vector2f center, bool instant = false);

    /*
     * Eases toward a target -- use for following a player.
     * `smoothing` is the fraction of the remaining distance covered per
     * second, roughly; higher is snappier.
     */
    void follow(sf::Vector2f target, sf::Time delta_time, float smoothing = 5.0f);

    /*
     * Adds trauma. Values around 0.2 read as a light hit, 0.5 as a
     * solid one, 1.0 as a screen-filling explosion. Trauma is capped at
     * 1, so spamming it cannot make the camera fly off.
     */
    void shake(float amount);

    /*
     * Holds a shake floor for as long as you keep calling it -- an
     * engine rumble, a minigun, a collapsing building. Call it every
     * frame while the source is active; it falls away on its own once
     * you stop.
     *
     * This exists because impulses alone cannot model continuous fire.
     * Trauma decays linearly, so a stream of small impulses either
     * fades to nothing between shots or, if they come faster than the
     * decay, climbs to maximum and stays pinned -- there is no stable
     * middle. A floor gives you the middle, and impulses still spike
     * above it and settle back down to it.
     *
     *     if(firing) camera.set_sustained_shake(0.28f);   // every frame
     *     if(explosion) camera.shake(0.8f);               // once
     */
    void set_sustained_shake(float amount);

    /*
     * How fast the sustained floor falls away when it stops being
     * refreshed. Higher is snappier. Defaults to 5 per second.
     */
    void set_sustained_decay(float per_second);

    [[nodiscard]] float get_sustained_shake() const;

    /*
     * Immediately stops all shaking.
     */
    void clear_shake();

    void set_size(sf::Vector2f size);
    void set_zoom(float factor);

    void set_transition(TransitionFunction transition);
    void set_transition_duration(float duration);

    void set_trauma_decay(float per_second);
    void set_max_shake_offset(float pixels);
    void set_max_shake_roll(float degrees);
    void set_shake_frequency(float hz);

    /*
     * Keeps the view inside a world rectangle. Pass nullopt to disable.
     */
    void set_bounds(std::optional<sf::FloatRect> bounds);

    [[nodiscard]] sf::View&       get_view();
    [[nodiscard]] const sf::View& get_view() const;

    /*
     * A screen-space view carrying the shake but NOT the world
     * position. Draw your HUD through this and it shakes with the
     * world while staying pinned to the screen -- the tank cannon
     * firing and the whole display jolting, rather than only the
     * world layer moving under a rock-steady HUD.
     *
     *     target.setView(m_camera.get_view());       // world: pans + shakes
     *     target.draw(m_particles);
     *
     *     target.setView(m_camera.get_ui_view());    // HUD: shakes only
     *     target.draw(m_widgets);
     *
     * For a HUD that ignores shake entirely, use
     * target.getDefaultView() as before.
     */
    [[nodiscard]] sf::View get_ui_view() const;

    /*
     * Shake components on their own, if you want to apply them to
     * something the view system does not cover.
     */
    [[nodiscard]] sf::Vector2f get_shake_offset() const;
    [[nodiscard]] sf::Angle    get_shake_roll() const;

    /*
     * Turns the rotational component of shake off. Roll is what makes
     * a hit feel weighty, but it also rotates the whole view, so
     * screen-space hit testing drifts further from the cursor the
     * further you are from the centre. Off by default for that reason.
     */
    void set_shake_roll_enabled(bool enabled);

    /*
     * Round the final view centre to whole pixels. ON by default.
     *
     * Without it, a sub-pixel shake offset produces a strange effect:
     * solid shapes sit on exact pixel boundaries and do not appear to
     * move at all, while text and textures -- which are sampled from an
     * atlas -- shift visibly. The result is that a gentle shake looks
     * like only the text is wobbling. Snapping makes everything move
     * together in whole-pixel steps, or not at all.
     */
    void set_pixel_snap(bool enabled);

    /*
     * Centre of the camera, shake included.
     */
    [[nodiscard]] sf::Vector2f get_center() const;

    /*
     * Centre the camera is easing toward, shake excluded. Use this for
     * gameplay logic that should not jitter.
     */
    [[nodiscard]] sf::Vector2f get_target_center() const;

    /*
     * Top-left corner of the visible area, in world space.
     */
    [[nodiscard]] sf::Vector2f get_position() const;

    /*
     * Visible world rectangle.
     */
    [[nodiscard]] sf::FloatRect get_viewport_bounds() const;

    [[nodiscard]] float get_trauma() const;
};

#endif
