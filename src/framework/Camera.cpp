/**
 * Camera.cpp
 *
 * Written by James Bouchat
 */

#include "../../include/framework/Camera.hpp"

namespace
{
    /*
     * Cheap deterministic hash, [-1, 1].
     */
    float hash_noise(int x, float seed)
    {
        float const n = std::sin(static_cast<float>(x) * 12.9898f + seed * 78.233f)
                      * 43758.5453f;
        return 2.0f * (n - std::floor(n)) - 1.0f;
    }

    /*
     * 1D value noise with smoothstep interpolation between integer
     * samples. Sampling fresh randomness every frame gives a harsh
     * buzz; interpolating between samples gives the camera a physical
     * sweep, which is what a real shake looks like.
     */
    float value_noise(float t, float seed)
    {
        float const floor_t = std::floor(t);
        int   const i       = static_cast<int>(floor_t);
        float const frac    = t - floor_t;

        float const smooth = frac * frac * (3.0f - 2.0f * frac);

        return hash_noise(i, seed) * (1.0f - smooth)
             + hash_noise(i + 1, seed) * smooth;
    }
}

Camera::Camera(sf::Vector2f size, sf::Vector2f center)
    : m_size(size)
{
    m_center.set_instant(center);

    m_noise_seed_x = Random::real(0.0f, 100.0f);
    m_noise_seed_y = Random::real(0.0f, 100.0f);
    m_noise_seed_r = Random::real(0.0f, 100.0f);

    m_view.setSize(m_size);
    m_view.setCenter(center);
}

void Camera::update(sf::Time delta_time)
{
    float const dt = delta_time.asSeconds();

    m_shake_time += dt;

    // Linear decay. Trauma is what accumulates; the visible offset is
    // its square, so shakes fall off sharply near the end rather than
    // trailing away.
    m_trauma = std::max(0.0f, m_trauma - m_trauma_decay * dt);

    /*
     * The sustained floor holds at exactly what was requested this
     * frame, and only decays once nothing is asking for it any more.
     *
     * Decaying unconditionally and letting the request top it back up
     * makes the level sawtooth between the requested value and one
     * frame's worth of decay below it, and since the visible offset
     * is the square, that reads as a flutter rather than a steady
     * rumble.
     */
    if(m_sustained_request >= m_sustained)
    {
        m_sustained = m_sustained_request;
    }
    else
    {
        m_sustained = std::max(m_sustained_request,
                               m_sustained - m_sustained_decay * dt);
    }

    // Must be re-requested every frame, so letting go of the trigger
    // needs no explicit "stop" call.
    m_sustained_request = 0.0f;

    // Impulses ride on top of the floor rather than replacing it, so an
    // explosion during sustained fire spikes and then settles back to
    // the rumble instead of to zero.
    float const total = std::clamp(m_trauma + m_sustained, 0.0f, 1.0f);
    float const shake = total * total;

    if(shake > 0.0f)
    {
        float const t = m_shake_time * m_shake_frequency;

        m_shake_offset = { value_noise(t, m_noise_seed_x) * m_max_offset * shake,
                           value_noise(t, m_noise_seed_y) * m_max_offset * shake };

        /*
         * Whole-pixel offsets only.
         *
         * A sub-pixel offset moves glyphs and textures (they resample)
         * but not solid shapes (they still cover the same pixels), so a
         * light shake looked like only the text was moving.
         */
        if(m_pixel_snap)
        {
            m_shake_offset = { std::round(m_shake_offset.x),
                               std::round(m_shake_offset.y) };
        }

        m_shake_roll = m_roll_enabled
                     ? sf::degrees(value_noise(t, m_noise_seed_r) * m_max_roll * shake)
                     : sf::Angle::Zero;
    }
    else
    {
        m_shake_offset = {0.0f, 0.0f};
        m_shake_roll   = sf::Angle::Zero;
    }

    // The tweened position is never written back to, so the tween runs
    // to completion undisturbed.
    sf::Vector2f base = clamp_to_bounds(m_center.get_value());

    // Snap the settled position too, so panning is also pixel-exact.
    if(m_pixel_snap) base = {std::round(base.x), std::round(base.y)};

    m_view.setCenter(base + m_shake_offset);
    m_view.setRotation(m_shake_roll);
}

sf::Vector2f Camera::clamp_to_bounds(sf::Vector2f center) const
{
    if(!m_bounds) return center;

    sf::FloatRect const& b = *m_bounds;
    sf::Vector2f const half = m_size / 2.0f;

    // If the world is smaller than the view on an axis, centre on it
    // rather than clamping to a nonsensical range.
    float const x = (b.size.x <= m_size.x)
                  ? b.position.x + b.size.x / 2.0f
                  : std::clamp(center.x, b.position.x + half.x,
                                         b.position.x + b.size.x - half.x);

    float const y = (b.size.y <= m_size.y)
                  ? b.position.y + b.size.y / 2.0f
                  : std::clamp(center.y, b.position.y + half.y,
                                         b.position.y + b.size.y - half.y);

    return {x, y};
}

void Camera::move(sf::Vector2f displacement)
{
    m_center.set_value(m_center.get_target() + displacement);
}

void Camera::set_center(sf::Vector2f center, bool instant)
{
    if(instant) m_center.set_instant(center);
    else        m_center.set_value(center);
}

void Camera::follow(sf::Vector2f target, sf::Time delta_time, float smoothing)
{
    // Exponential smoothing, framerate-independent.
    float const alpha = 1.0f - std::exp(-smoothing * delta_time.asSeconds());
    sf::Vector2f const current = m_center.get_value();

    m_center.set_instant(current + (target - current) * alpha);
}

void Camera::shake(float amount)
{
    // Additive, so overlapping sources stack instead of replacing each
    // other, and capped so they cannot run away.
    m_trauma = std::clamp(m_trauma + amount, 0.0f, 1.0f);
}

void Camera::set_sustained_shake(float amount)
{
    // Take the strongest source asking this frame, rather than summing
    //, two rumbling engines should not shake twice as hard.
    m_sustained_request = std::clamp(std::max(m_sustained_request, amount),
                                     0.0f, 1.0f);
}

void Camera::set_sustained_decay(float per_second)
{
    m_sustained_decay = std::max(0.0f, per_second);
}

float Camera::get_sustained_shake() const
{
    return m_sustained;
}

void Camera::clear_shake()
{
    m_trauma            = 0.0f;
    m_sustained         = 0.0f;
    m_sustained_request = 0.0f;
    m_shake_offset = {0.0f, 0.0f};
    m_shake_roll   = sf::Angle::Zero;
}

void Camera::set_size(sf::Vector2f size)
{
    m_size = size;
    m_view.setSize(m_size);
}

void Camera::set_zoom(float factor)
{
    if(factor <= 0.0f) return;
    m_view.setSize(m_size / factor);
}

void Camera::set_transition(TransitionFunction transition)
{
    m_center.set_transition(transition);
}

void Camera::set_transition_duration(float duration)
{
    m_center.set_duration(duration);
}

void Camera::set_trauma_decay(float per_second)
{
    m_trauma_decay = std::max(0.0f, per_second);
}

void Camera::set_max_shake_offset(float pixels)
{
    m_max_offset = pixels;
}

void Camera::set_max_shake_roll(float degrees)
{
    m_max_roll = degrees;
}

void Camera::set_shake_frequency(float hz)
{
    m_shake_frequency = std::max(0.0f, hz);
}

void Camera::set_bounds(std::optional<sf::FloatRect> bounds)
{
    m_bounds = bounds;
}

sf::View& Camera::get_view()
{
    return m_view;
}

sf::View Camera::get_ui_view() const
{
    // Centred on the screen, displaced by the shake only. Content drawn
    // through this stays in screen space but jolts with the world.
    sf::View ui;
    ui.setSize(m_size);
    ui.setCenter(m_size / 2.0f + m_shake_offset);
    ui.setRotation(m_shake_roll);
    return ui;
}

sf::Vector2f Camera::get_shake_offset() const
{
    return m_shake_offset;
}

sf::Angle Camera::get_shake_roll() const
{
    return m_shake_roll;
}

void Camera::set_shake_roll_enabled(bool enabled)
{
    m_roll_enabled = enabled;
    if(!m_roll_enabled) m_shake_roll = sf::Angle::Zero;
}

void Camera::set_pixel_snap(bool enabled)
{
    m_pixel_snap = enabled;
}

const sf::View& Camera::get_view() const
{
    return m_view;
}

sf::Vector2f Camera::get_center() const
{
    return m_view.getCenter();
}

sf::Vector2f Camera::get_target_center() const
{
    return m_center.get_target();
}

sf::Vector2f Camera::get_position() const
{
    return m_view.getCenter() - m_view.getSize() / 2.0f;
}

sf::FloatRect Camera::get_viewport_bounds() const
{
    return sf::FloatRect{get_position(), m_view.getSize()};
}

float Camera::get_trauma() const
{
    // The effective level, floor included, this is what actually
    // drives the visible offset.
    return std::clamp(m_trauma + m_sustained, 0.0f, 1.0f);
}
