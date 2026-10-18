/**
 * Effects.cpp  -  Tetris
 */

#include "Effects.hpp"

namespace tetris
{

void push_quad(sf::VertexArray& va, sf::Vector2f pos, sf::Vector2f size,
               sf::Color color)
{
    if(size.x <= 0.0f || size.y <= 0.0f || color.a == 0) return;

    sf::Vector2f const a{pos.x,          pos.y};
    sf::Vector2f const b{pos.x + size.x, pos.y};
    sf::Vector2f const c{pos.x + size.x, pos.y + size.y};
    sf::Vector2f const d{pos.x,          pos.y + size.y};

    for(sf::Vector2f v : {a, b, c, a, c, d})
        va.append(sf::Vertex{v, color});
}

void push_outline(sf::VertexArray& va, sf::Vector2f pos, sf::Vector2f size,
                  float thickness, sf::Color color)
{
    if(thickness <= 0.0f) return;

    push_quad(va, pos, {size.x, thickness}, color);
    push_quad(va, {pos.x, pos.y + size.y - thickness}, {size.x, thickness}, color);
    push_quad(va, {pos.x, pos.y + thickness}, {thickness, size.y - thickness * 2.0f}, color);
    push_quad(va, {pos.x + size.x - thickness, pos.y + thickness},
              {thickness, size.y - thickness * 2.0f}, color);
}

void push_block(sf::VertexArray& va, sf::Vector2f cell_pos, float cell_size,
                sf::Color color, float alpha)
{
    if(alpha <= 0.0f) return;

    float const inset = 1.0f;
    float const bevel = std::max(1.0f, cell_size * 0.14f);
    float const body  = cell_size - inset * 2.0f;

    sf::Vector2f const origin{cell_pos.x + inset, cell_pos.y + inset};

    sf::Color const base    = with_alpha(color, alpha);
    sf::Color const light   = with_alpha(mix(color, FG0, 0.34f), alpha);
    sf::Color const shadow  = with_alpha(scaled(color, 0.62f), alpha);

    // Body.
    push_quad(va, origin, {body, body}, base);

    // Top and left catch the light.
    push_quad(va, origin, {body, bevel}, light);
    push_quad(va, {origin.x, origin.y + bevel}, {bevel, body - bevel}, light);

    // Bottom and right fall away.
    push_quad(va, {origin.x + bevel, origin.y + body - bevel},
              {body - bevel, bevel}, shadow);
    push_quad(va, {origin.x + body - bevel, origin.y + bevel},
              {bevel, body - bevel * 2.0f}, shadow);
}

void push_ghost(sf::VertexArray& va, sf::Vector2f cell_pos, float cell_size,
                sf::Color color, float alpha)
{
    if(alpha <= 0.0f) return;

    float const inset = 2.0f;
    push_outline(va, {cell_pos.x + inset, cell_pos.y + inset},
                 {cell_size - inset * 2.0f, cell_size - inset * 2.0f},
                 1.0f, with_alpha(color, alpha * 0.55f));
}

// ----------------------------------------------------------------- Popup

Popup::Popup(const sf::Font& font, const std::string& message,
             sf::Vector2f position, sf::Color color,
             unsigned character_size, float life)
    : m_text(font, message, character_size),
      m_origin(position),
      m_life(life)
{
    m_text.setFillColor(color);

    sf::FloatRect const bounds = m_text.getLocalBounds();
    m_text.setOrigin({bounds.size.x / 2.0f, bounds.size.y / 2.0f});

    // Overshoot on the way in gives the callout a bit of snap.
    m_scale.set_transition(TransitionFunction::EaseOutBack);
    m_scale.set_duration(0.28f);
    m_scale.set_instant(0.4f);
    m_scale = 1.0f;
}

void Popup::update(sf::Time delta_time)
{
    m_age += delta_time.asSeconds();
}

void Popup::draw(sf::RenderTarget& target) const
{
    float const t = std::clamp(m_age / m_life, 0.0f, 1.0f);

    // Hold at full opacity for the first two thirds, then fade.
    float const fade = (t < 0.65f) ? 1.0f : 1.0f - (t - 0.65f) / 0.35f;

    sf::Text copy = m_text;

    sf::Color color = copy.getFillColor();
    copy.setFillColor(with_alpha(color, std::clamp(fade, 0.0f, 1.0f)));

    float const scale = m_scale.get_value();
    copy.setScale({scale, scale});

    // Drifts upward as it goes, which reads as "released" rather than
    // just vanishing.
    copy.setPosition(floor_vector({m_origin.x, m_origin.y - m_rise * t * t}));

    target.draw(copy);
}

// ------------------------------------------------------------- PopupList

void PopupList::add(const sf::Font& font, const std::string& message,
                    sf::Vector2f position, sf::Color color,
                    unsigned character_size, float life)
{
    m_popups.emplace_back(font, message, position, color, character_size, life);
}

void PopupList::update(sf::Time delta_time)
{
    for(Popup& popup : m_popups) popup.update(delta_time);
    std::erase_if(m_popups, [](const Popup& p) { return p.finished(); });
}

void PopupList::draw(sf::RenderTarget& target) const
{
    for(const Popup& popup : m_popups) popup.draw(target);
}

void PopupList::clear()
{
    m_popups.clear();
}

// ---------------------------------------------------------- AmbientField

AmbientField::AmbientField(sf::Vector2f bounds, int count)
    : m_bounds(bounds)
{
    m_motes.reserve(static_cast<std::size_t>(std::max(0, count)));

    for(int i = 0; i < count; ++i)
    {
        Mote mote;
        mote.pos        = {Random::real(0.0f, bounds.x), Random::real(0.0f, bounds.y)};
        mote.speed      = Random::real(4.0f, 16.0f);
        mote.radius     = Random::real(1.0f, 2.4f);
        mote.phase      = Random::real(0.0f, 6.28f);
        mote.brightness = Random::real(0.16f, 0.42f);
        m_motes.push_back(mote);
    }
}

void AmbientField::update(sf::Time delta_time, float energy)
{
    float const dt = delta_time.asSeconds();

    m_vertices.clear();

    for(Mote& mote : m_motes)
    {
        // Rise, with a slow lateral sway so the field does not read as
        // a uniform sheet moving in one direction.
        mote.pos.y -= mote.speed * (1.0f + energy) * dt;
        mote.pos.x += std::sin(GameTime::real_now() * 0.4f + mote.phase) * 6.0f * dt;

        if(mote.pos.y < -4.0f)
        {
            mote.pos.y = m_bounds.y + 4.0f;
            mote.pos.x = Random::real(0.0f, m_bounds.x);
        }
        if(mote.pos.x < -4.0f)          mote.pos.x = m_bounds.x + 4.0f;
        if(mote.pos.x > m_bounds.x + 4.0f) mote.pos.x = -4.0f;

        float const pulse = 0.75f + 0.25f * std::sin(GameTime::real_now() * 1.6f + mote.phase);
        float const alpha = mote.brightness * pulse * (0.6f + energy * 0.6f);

        sf::Color const color = with_alpha(mix(BG4, FG4, energy), alpha);

        push_quad(m_vertices, {mote.pos.x - mote.radius, mote.pos.y - mote.radius},
                  {mote.radius * 2.0f, mote.radius * 2.0f}, color);
    }
}

void AmbientField::draw(sf::RenderTarget& target) const
{
    if(m_vertices.getVertexCount() == 0) return;
    target.draw(m_vertices);
}

}
