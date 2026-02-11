#include "../../include/scenes/MenuScene.hpp"

MenuScene::MenuScene()
{

}

MenuScene::~MenuScene()
{
}

void MenuScene::on_enter()
{
    std::cout << "Entering Menu Scene" << std::endl;
}

void MenuScene::on_exit()
{
    std::cout << "Exiting Menu Scene" << std::endl;
}

void MenuScene::process_events()
{
    while(const std::optional event = m_render_window->pollEvent())
    {
        if(event->is<sf::Event::Closed>())
        {
            m_render_window->close();
        }
        
        if(sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Num2))
        {
            EventBus::publish(Event(EventType::SWITCH_TO_GAME, ""));
        }
    }
}

void MenuScene::update(sf::Time delta_time)
{

}

void MenuScene::render()
{
    m_render_texture->clear(sf::Color::Blue);
    m_render_texture->display();
    
    m_render_sprite->setTexture(m_render_texture->getTexture());
}
