#include "../../include/scenes/GameScene.hpp"

GameScene::GameScene() :
    m_particle_system_manager(sf::Vector2f(0, 0))
{

}

void GameScene::on_enter()
{
    std::cout << "Entering Game Scene" << std::endl;
    ParticleSystem p(sf::Vector2f(WINDOW_WIDTH/2, WINDOW_HEIGHT/2));
    p.set_lifespan(sf::seconds(2));
    p.set_drag(0.95);
    p.toggle_fade();
    m_particle_system_manager.add_particle_system("mouse_particles", std::move(p));
}

void GameScene::on_exit()
{
    std::cout << "Exiting Game Scene" << std::endl;
    m_particle_system_manager.remove_particle_system("mouse_particles");
}

void GameScene::process_events()
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

        if(sf::Mouse::isButtonPressed(sf::Mouse::Button::Left))
        {
            m_particle_system_manager.set_particle_system_position("mouse_particles", world_pos);

            int num_particles = 200;

            std::vector<sf::Color> colors = {sf::Color(203, 53, 61), sf::Color::Red, sf::Color(237, 98, 64), sf::Color(86, 61, 67), sf::Color(255, 255, 255)};
            for(int i = 0; i < num_particles; ++i)
            {
                int rand_color = rand() % colors.size();
                std::normal_distribution rand_speed{3.0, 5.0};
                float speed = rand_speed(random_gen);
                m_particle_system_manager.get_particle_system("mouse_particles")->add_particles(1, colors[rand_color], Vector{speed, sf::degrees(0)}, 180);
            }
        }
    }
}

void GameScene::update(sf::Time delta_time)
{
    m_particle_system_manager.update(delta_time);
}

void GameScene::render()
{
    m_render_texture->clear(sf::Color(200, 200, 200));

    m_render_texture->draw(m_particle_system_manager);

    m_render_texture->display();
    
    m_render_sprite->setTexture(m_render_texture->getTexture());
}

