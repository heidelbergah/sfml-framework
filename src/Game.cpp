/**
 * Game.cpp
 *
 * Written by James Bouchat
 */

#include "../include/framework/Game.hpp"

void Game::process_events()
{
    while(const std::optional event = m_window.pollEvent())
    {
        if(event->is<sf::Event::Closed>())
        {
            m_window.close();
        }
    }

}

void Game::update(sf::Time delta_time)
{
    float delta_time_seconds = delta_time.asSeconds();
    int speed = 100;

    std::cout << speed * delta_time_seconds << std::endl;

    if(sf::Keyboard::isKeyPressed(sf::Keyboard::Key::W))
    {
        m_player.setPosition(sf::Vector2f(m_player.getPosition().x, m_player.getPosition().y - speed * delta_time_seconds));
    }
    if(sf::Keyboard::isKeyPressed(sf::Keyboard::Key::S))
    {
        m_player.setPosition(sf::Vector2f(m_player.getPosition().x, m_player.getPosition().y + speed * delta_time_seconds));
    }
    if(sf::Keyboard::isKeyPressed(sf::Keyboard::Key::A))
    {
        m_player.setPosition(sf::Vector2f(m_player.getPosition().x - speed * delta_time_seconds, m_player.getPosition().y));
    }
    if(sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D))
    {
        m_player.setPosition(sf::Vector2f(m_player.getPosition().x + speed * delta_time_seconds, m_player.getPosition().y));
    }

}

void Game::render()
{
    m_window.clear();
    m_window.draw(m_player);
    m_window.display();
}

Game::Game()
{
    // ===== DO NOT REMOVE FUNCTION CALLS ===== //
    m_window.create(sf::VideoMode({WINDOW_WIDTH, WINDOW_HEIGHT}), GAME_NAME);
    // ======================================== //
    
    m_player.setFillColor(sf::Color::Blue);
    m_player.setRadius(150);
    m_player.setPosition(sf::Vector2f(50, 50));
}

void Game::run(int frame_per_seconds)
{
    // RUN USES THE FIXED TIME STEPS APPROACH
    sf::Time time_since_last_update = sf::Time::Zero;
    sf::Time time_per_frame = sf::seconds(1.f / frame_per_seconds);

    while(m_window.isOpen())
    {
        process_events();
        
        bool repaint = false;

        time_since_last_update += m_clock.restart();
        while(time_since_last_update > time_per_frame)
        {
            time_since_last_update -= time_per_frame;
            repaint = true;
            update(time_per_frame);
        }
        if(repaint)
        {
            render();
        }
    }
}
