/**
 * Effects.hpp  -  Cascade
 *
 * Drawing helpers and the floating-text pop-ups.
 *
 * Everything on the board is pushed into a handful of vertex arrays
 * rather than drawn as individual shapes. A full board plus ghost plus
 * queue is around three hundred blocks; as separate sf::RectangleShape
 * draws that would be three hundred draw calls a frame, and as
 * triangles it is three.
 */

#ifndef TETRIS_EFFECTS_HPP
#define TETRIS_EFFECTS_HPP

#include "Palette.hpp"

namespace tetris
{
    /*
     * Axis-aligned filled rectangle, as two triangles.
     */
    void push_quad(sf::VertexArray& va, sf::Vector2f pos, sf::Vector2f size,
                   sf::Color color);

    /*
     * A rectangular outline of the given thickness, drawn inward.
     */
    void push_outline(sf::VertexArray& va, sf::Vector2f pos, sf::Vector2f size,
                      float thickness, sf::Color color);

    /*
     * One playfield block: a body inset inside its cell, a lighter bevel
     * along the top and left, and a darker one along the bottom and
     * right. Cheap, and it reads as solid rather than flat.
     */
    void push_block(sf::VertexArray& va, sf::Vector2f cell_pos, float cell_size,
                    sf::Color color, float alpha = 1.0f);

    /*
     * Ghost piece: outline only, no body.
     */
    void push_ghost(sf::VertexArray& va, sf::Vector2f cell_pos, float cell_size,
                    sf::Color color, float alpha = 1.0f);

    /*
     * Floating text that pops in, holds, then drifts up and fades.
     * Used for TETRIS / T-SPIN / combo callouts.
     */
    class Popup
    {
    private:
        sf::Text  m_text;
        sf::Vector2f m_origin;

        Interpolated<float> m_scale;
        float m_age{0.0f};
        float m_life{1.4f};
        float m_rise{34.0f};

    public:
        Popup(const sf::Font& font, const std::string& message,
              sf::Vector2f position, sf::Color color,
              unsigned character_size = 16, float life = 1.4f);

        void update(sf::Time delta_time);
        void draw(sf::RenderTarget& target) const;

        [[nodiscard]] bool finished() const { return m_age >= m_life; }
    };

    /*
     * Owns the live pop-ups and retires them when they expire.
     */
    class PopupList
    {
    private:
        std::vector<Popup> m_popups;

    public:
        void add(const sf::Font& font, const std::string& message,
                 sf::Vector2f position, sf::Color color,
                 unsigned character_size = 16, float life = 1.4f);

        void update(sf::Time delta_time);
        void draw(sf::RenderTarget& target) const;
        void clear();

        [[nodiscard]] std::size_t size() const { return m_popups.size(); }
    };

    /*
     * Slow drifting motes behind the board. Deliberately dim -- this is
     * the one effect that is on screen constantly, so it has to sit
     * well below everything else in contrast.
     */
    class AmbientField
    {
    private:
        struct Mote
        {
            sf::Vector2f pos;
            float        speed{0.0f};
            float        radius{0.0f};
            float        phase{0.0f};
            float        brightness{0.0f};
        };

        std::vector<Mote> m_motes;
        sf::VertexArray   m_vertices{sf::PrimitiveType::Triangles};
        sf::Vector2f      m_bounds;

    public:
        AmbientField(sf::Vector2f bounds, int count);

        void update(sf::Time delta_time, float energy);
        void draw(sf::RenderTarget& target) const;
    };
}

#endif
