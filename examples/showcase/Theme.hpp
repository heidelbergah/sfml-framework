/**
 * Theme.hpp  -  Showcase
 *
 * Palette, font and a few widget helpers shared by every demo, so the
 * scenes themselves only describe what is on screen.
 *
 * The font is Pixel Operator (CC0). It is drawn on a 16px grid, so text
 * stays crisp at 16 and 32 and nowhere in between.
 */

#ifndef SHOWCASE_THEME_HPP
#define SHOWCASE_THEME_HPP

#include "../../include/gui/Button.hpp"
#include "../../include/gui/Frame.hpp"
#include "../../include/gui/TextBox.hpp"

namespace showcase
{

// --------------------------------------------------------------- palette

inline const sf::Color INK      {21, 19, 26};      // background
inline const sf::Color PANEL    {32, 29, 38};
inline const sf::Color PANEL_HI {46, 42, 54};
inline const sf::Color LINE     {68, 62, 78};
inline const sf::Color TEXT     {214, 203, 182};   // kept under the bloom threshold
inline const sf::Color TEXT_DIM {128, 118, 120};

inline const sf::Color AMBER    {240, 170, 78};
inline const sf::Color CORAL    {222, 106, 88};
inline const sf::Color SAGE     {132, 180, 140};
inline const sf::Color SKY      {112, 162, 204};
inline const sf::Color ROSE     {206, 128, 164};

inline constexpr float VIEW_W = static_cast<float>(WINDOW_WIDTH);
inline constexpr float VIEW_H = static_cast<float>(WINDOW_HEIGHT);

inline constexpr unsigned TEXT_SIZE  = 16;
inline constexpr unsigned TITLE_SIZE = 32;

// ---------------------------------------------------------------- assets

inline const sf::Font* font()
{
    return Assets::font("assets/fonts/pixel-font-bitmap.ttf");
}

// --------------------------------------------------------------- helpers

/*
 * A line of text in a transparent box.
 */
inline std::shared_ptr<TextBox> make_label(sf::Vector2f size, const std::string& text,
                                           sf::Color color = TEXT,
                                           Alignment alignment = Alignment::LEFT,
                                           unsigned character_size = TEXT_SIZE)
{
    auto label = std::make_shared<TextBox>(size, sf::Color::Transparent, text,
                                           *font(), color);
    label->set_character_size(character_size);
    label->set_alignment(alignment);
    label->set_transition(TransitionFunction::None);
    return label;
}

/*
 * A flat button that brightens on hover and darkens when pressed.
 */
inline std::shared_ptr<Button> make_button(sf::Vector2f size, const std::string& text,
                                           sf::Color color = PANEL_HI,
                                           sf::Color text_color = TEXT)
{
    auto button = std::make_shared<Button>(size, color);
    button->add_text(text, *font(), text_color);
    button->set_character_size(TEXT_SIZE);
    button->set_state_tints(-16, 12);
    button->set_transition(TransitionFunction::None);
    return button;
}

/*
 * Title bar for every window in the showcase: the panel colour one
 * step up, dim text, and a close button that only shows itself on hover.
 */
inline TaskbarStyle taskbar_style()
{
    TaskbarStyle style;
    style.color            = PANEL_HI;
    style.text_color       = TEXT_DIM;
    style.close_color      = PANEL_HI;
    style.close_text_color = TEXT_DIM;
    style.close_label      = "x";
    style.character_size   = TEXT_SIZE;
    style.title_alignment  = Alignment::LEFT;
    return style;
}

/*
 * Recolours a button and rebuilds its hover and press tints, which
 * Button otherwise derives once from its constructor colour.
 */
inline void recolor(Button& button, sf::Color color)
{
    button.set_color(color);
    button.set_state_tints(-16, 12);
}

/*
 * Same colour, scaled alpha.
 */
inline sf::Color fade(sf::Color color, float alpha)
{
    color.a = static_cast<std::uint8_t>(std::clamp(alpha, 0.0f, 1.0f) * color.a);
    return color;
}

/*
 * Linear blend, t = 0 gives a, t = 1 gives b.
 */
inline sf::Color mix(sf::Color a, sf::Color b, float t)
{
    auto lerp = [t](std::uint8_t x, std::uint8_t y)
    {
        return static_cast<std::uint8_t>(static_cast<float>(x) +
                                         (static_cast<float>(y) - static_cast<float>(x)) * t);
    };
    return {lerp(a.r, b.r), lerp(a.g, b.g), lerp(a.b, b.b), lerp(a.a, b.a)};
}

/*
 * Appends an axis-aligned rectangle to a Triangles vertex array.
 */
inline void push_rect(sf::VertexArray& vertices, sf::FloatRect rect, sf::Color color)
{
    sf::Vector2f const a = rect.position;
    sf::Vector2f const b = {rect.position.x + rect.size.x, rect.position.y};
    sf::Vector2f const c = rect.position + rect.size;
    sf::Vector2f const d = {rect.position.x, rect.position.y + rect.size.y};

    for(sf::Vector2f p : {a, b, c, a, c, d})
        vertices.append(sf::Vertex{p, color});
}

}

#endif
