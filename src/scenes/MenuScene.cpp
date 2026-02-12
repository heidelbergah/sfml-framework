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
    if(!font.openFromFile("././assets/fonts/Windows-Regular.ttf"))
    {
        std::cerr << "Font cannot be opened" << std::endl;
    }
    font.setSmooth(false);

    m_widget_manager.add_widget("s_g_button", std::make_shared<Button>(sf::Vector2f(100.f, 30.f), sf::Color::White));
    m_widget_manager.get_widget<Button>("s_g_button")->set_position(sf::Vector2f(32, 32), true);
    m_widget_manager.get_widget<Button>("s_g_button")->add_text("Play Game", font, sf::Color::Black);
    m_widget_manager.get_widget<Button>("s_g_button")->set_outline(sf::Color::Black, 2);
    m_widget_manager.get_widget<Button>("s_g_button")->set_callback([this]()
            {
                EventBus::publish(Event(EventType::SWITCH_TO_GAME, ""));
            });
}

void MenuScene::on_exit()
{
    std::cout << "Exiting Menu Scene" << std::endl;
}

void MenuScene::process_events()
{
    sf::Vector2i pixel_pos = sf::Mouse::getPosition(*m_render_window);
    sf::Vector2f scaled_pos = {
        static_cast<float>(pixel_pos.x) / m_resolution_scale.x,
        static_cast<float>(pixel_pos.y) / m_resolution_scale.y
    };
    sf::Vector2f world_pos = m_render_texture->mapPixelToCoords(sf::Vector2i(scaled_pos));
    
    while(const std::optional event = m_render_window->pollEvent())
    {
        if(event->is<sf::Event::Closed>())
        {
            m_render_window->close();
        }
 
        for(const auto& widget : m_widget_manager.get_widgets())
        {
            widget->handle_event(*m_render_window, world_pos);
        }
    }
}

void MenuScene::update(sf::Time delta_time)
{
    for(const auto& widget : m_widget_manager.get_widgets())
    {
        widget->update(delta_time);
    }
}

void MenuScene::render()
{
    m_render_texture->clear(sf::Color(80, 80, 80));

    for(const auto& widget : m_widget_manager.get_widgets())
    {
        m_render_texture->draw(*widget);
    }

}
