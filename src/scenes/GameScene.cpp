#include "../../include/scenes/GameScene.hpp"

GameScene::GameScene() :
    m_particle_system_manager(sf::Vector2f(0, 0))
{

}

void GameScene::on_enter()
{
    std::cout << "Entering Game Scene" << std::endl;
}

void GameScene::on_exit()
{
    std::cout << "Exiting Game Scene" << std::endl;
}

void GameScene::process_events()
{

    while(const std::optional event = m_render_window->pollEvent())
    {
        if(event->is<sf::Event::Closed>())
        {
            m_render_window->close();
        }
    }
}

void GameScene::update(sf::Time delta_time)
{

}

void GameScene::render()
{
    m_render_texture->clear(sf::Color::Green);
    m_render_texture->display();
    
    m_render_sprite->setTexture(m_render_texture->getTexture());
}

