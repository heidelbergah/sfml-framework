/**
 * Level.hpp  -  Cinder
 *
 * What each level contains, as plain data. Everything decorative --
 * grass, flowers, trees, stones -- is scattered from a seed when the level
 * is built, so a level is just its shape: walls, water, wind, braziers
 * and the embers lying around.
 */

#ifndef CINDER_LEVEL_HPP
#define CINDER_LEVEL_HPP

#include <SFML/Graphics/Rect.hpp>
#include <SFML/System/Vector2.hpp>

#include <string>
#include <vector>

namespace cinder
{

struct Wind
{
    sf::FloatRect area;
    sf::Vector2f  push;     // pixels per second
};

struct LevelDef
{
    std::string name;
    std::string line;       // a few words shown under the name

    sf::Vector2f size;
    sf::Vector2f start;
    unsigned     seed{1};

    std::vector<sf::FloatRect> walls;
    std::vector<sf::FloatRect> water;
    std::vector<Wind>          winds;
    std::vector<sf::Vector2f>  braziers;
    std::vector<sf::Vector2f>  embers;

    float drain{0.02f};     // heat lost per second, just by existing
    bool  snow{false};
};

[[nodiscard]] const std::vector<LevelDef>& levels();

}

#endif
