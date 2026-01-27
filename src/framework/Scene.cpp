#include "../../include/framework/Scene.hpp"

Scene::Scene()
{
    
}

void Scene::run(int frame_per_seconds)
{
    sf::Time time_since_last_update = sf::Time::Zero;
    sf::Time time_per_frame = sf::seconds(1.f / frame_per_seconds);

    on_enter();

    while(m_window->isOpen())
    {
        handle_event();
        
        bool repaint = false;

        time_since_last_update += m_clock->restart();
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

    on_exit();
}
