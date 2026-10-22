/**
 * Widgets.hpp  -  Desk
 *
 * Two small widgets for putting things on paper.
 *
 * Paragraph wraps text to a width, top-left aligned, which TextBox does
 * not do. It can also black out words: anything in {braces} is drawn as
 * a censor's bar instead of text.
 *
 * Picture draws a little pixel illustration, built once from a function
 * that fills in a vertex array.
 */

#ifndef DESK_WIDGETS_HPP
#define DESK_WIDGETS_HPP

#include "../../include/gui/Widget.hpp"

namespace desk
{

class Paragraph : public Widget
{
private:
    std::vector<sf::Text> m_lines;
    sf::VertexArray       m_bars{sf::PrimitiveType::Triangles};

    void on_draw(sf::RenderTarget& target, sf::RenderStates states) const override;

public:
    /*
     * `text` may contain \n for paragraph breaks and {braces} around
     * anything that should be censored. Height is worked out from the
     * wrapped result; get_size() reports it.
     */
    Paragraph(float width, const std::string& text, const sf::Font& font,
              unsigned size, sf::Color color, float line_spacing = 1.0f,
              bool centred = false);
};

class Picture : public Widget
{
public:
    using Builder = std::function<void(sf::VertexArray&, sf::Vector2f size)>;

private:
    sf::VertexArray m_vertices{sf::PrimitiveType::Triangles};

    void on_draw(sf::RenderTarget& target, sf::RenderStates states) const override;

public:
    Picture(sf::Vector2f size, const Builder& build);
};

/*
 * Appends an axis-aligned rectangle to a Triangles array.
 */
void push_rect(sf::VertexArray& vertices, sf::FloatRect rect, sf::Color color);

}

#endif
