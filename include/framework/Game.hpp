/**
 * Game.hpp
 *
 * Written by James Bouchat
 */

#ifndef GAME_HPP
#define GAME_HPP

#include "../Includes.hpp"
#include "../gui/Widget.hpp"
#include "../gui/Button.hpp"
#include "../gui/TextBox.hpp"
#include "../gui/Frame.hpp"
#include "../gui/Slider.hpp"
#include "../gui/Gauge.hpp"

class Game
{
private:
    // THIS PRIVATE SECTION MUST REMAIN IN GAME CLASS, DO NOT REMOVE
    sf::RenderWindow m_window;
    sf::Clock m_clock;
    const std::string GAME_NAME = "undefined";
    const unsigned int WINDOW_WIDTH = 800, WINDOW_HEIGHT = 800;

    void initialize_objects();
    void process_events();
    void update(sf::Time delta_time);
    void render();

private:
    // ADD YOUR DATA HERE
    std::vector<std::unique_ptr<Widget>> m_widgets;

public:
    Game();
    ~Game();

    void run(int frame_per_seconds);

};

#endif
