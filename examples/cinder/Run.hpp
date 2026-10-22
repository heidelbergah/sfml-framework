/**
 * Run.hpp  -  Cinder
 *
 * What carries between scenes: how the last run went, for the ending,
 * and the font every scene uses.
 */

#ifndef CINDER_RUN_HPP
#define CINDER_RUN_HPP

#include "../../include/core/AssetCache.hpp"

#include <SFML/Graphics/Font.hpp>
#include <SFML/Graphics/Text.hpp>

#include <cmath>
#include <string>

namespace cinder
{

struct Run
{
    float time{0.0f};
    int   deaths{0};
    int   embers{0};
    bool  finished{false};

    static Run& current()
    {
        static Run run;
        return run;
    }
};

inline const sf::Font* font()
{
    return Assets::font("assets/fonts/pixel-font-bitmap.ttf");
}

/*
 * Text centred on `centre`, snapped to whole pixels so the bitmap font
 * stays crisp. Sizes should be multiples of 16.
 */
inline sf::Text centred_text(const sf::Font& f, const std::string& s, unsigned size,
                             sf::Vector2f centre, sf::Color colour)
{
    sf::Text text(f, s, size);
    text.setFillColor(colour);
    sf::FloatRect const b = text.getLocalBounds();
    text.setOrigin({std::round(b.position.x + b.size.x * 0.5f), std::round(b.position.y + b.size.y * 0.5f)});
    text.setPosition({std::round(centre.x), std::round(centre.y)});
    return text;
}

}

#endif
