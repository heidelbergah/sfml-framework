/**
 * Game.hpp
 *
 * Written by James Bouchat
 */

#ifndef GAME_HPP
#define GAME_HPP

#include <SFML/Window.hpp>
#include <SFML/System.hpp>
#include <SFML/Graphics.hpp>
//#include <SFML/Audio.hpp>
//#include <SFML/Network.hpp>

#include <iostream>
//#include <vector>

class Game
{
private:
    // THIS PRIVATE SECTION MUST REMAIN IN GAME CLASS, DO NOT REMOVE
    sf::RenderWindow m_window;
    sf::Clock m_clock;
    const std::string GAME_NAME = "undefined";
    const unsigned int WINDOW_WIDTH = 800, WINDOW_HEIGHT = 800;

    void process_events();
    void update(sf::Time delta_time);
    void render();

private:
    // ADD YOUR DATA HERE
    sf::CircleShape m_player;

public:
    Game();
    void run(int frame_per_seconds);

};

#endif
