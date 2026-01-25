/**
 * Game.cpp
 *
 * Written by James Bouchat
 */

#include "../../include/framework/Game.hpp"

void print()
{
    std::cout << "Callback function" << std::endl;
}

void Game::initialize_objects()
{
    sf::Font font("././assets/fonts/Windows-Regular.ttf");
    font.setSmooth(false);
    sf::Texture texture("././assets/textures/simple_button.png");

    /** INITIALIZE GUI WIDGETS **/
    m_widgets.add_widget("button", std::make_shared<Button>(sf::Vector2f(64, 64), sf::Color::White));
    m_widgets.get_widget<Button>("button")->set_position(sf::Vector2f(32, 32), true);
    m_widgets.get_widget<Button>("button")->add_text("Click Me!", font, sf::Color::Black);
    m_widgets.get_widget<Button>("button")->set_transition(TransitionFunction::EaseOutElastic);
    m_widgets.get_widget<Button>("button")->set_background_texture(texture);

    m_widgets.add_widget("frame", std::make_shared<Frame>(sf::Vector2f(128, 128), sf::Color::Blue, font));
    m_widgets.get_widget<Frame>("frame")->set_position(sf::Vector2f(96, 96), true);
    m_widgets.get_widget<Frame>("frame")->set_outline(sf::Color::Black, 2);
    m_widgets.get_widget<Frame>("frame")->add_taskbar(12, sf::Color::White, sf::Color::Black, 2, "Big Frame");
    m_widgets.get_widget<Frame>("frame")->set_z_value(10);
    m_widgets.get_widget<Frame>("frame")->set_transition_duration(0.5f);

    m_widgets.add_widget("text", std::make_shared<TextBox>(sf::Vector2f(160, 160), sf::Color::White, "Press 1 & 2 to move button.\nPress Enter to toggle bar.\nPress space to fire bullets.", font, sf::Color::Black));;
    m_widgets.get_widget<TextBox>("text")->set_position(sf::Vector2f(128, 256), true);
    m_widgets.get_widget<TextBox>("text")->set_outline(sf::Color::Red, 2);
    m_widgets.get_widget<TextBox>("text")->set_z_value(20);
    m_widgets.get_widget<TextBox>("text")->add_text_scroll(20);

    m_widgets.add_widget("slider", std::make_shared<Slider>(sf::Vector2f(100, 10), sf::Color::Yellow));
    m_widgets.get_widget<Slider>("slider")->set_position(sf::Vector2f(256, 32));

    /** INITIALIZE PARTICLE SYSTEMS **/
    ParticleSystem ps1(m_widgets.get_widget<Button>("button")->get_position());
    //ps1.toggle_gravity();
    ps1.toggle_fade();
    ps1.set_drag(0.96f);
    ps1.set_lifespan(sf::seconds(4));

    ParticleSystem bullets(sf::Vector2f(WINDOW_WIDTH, WINDOW_HEIGHT/2));
    bullets.set_lifespan(sf::seconds(4));

    m_particle_system_manager.add_particle_system("bp", std::move(ps1));
    m_particle_system_manager.add_particle_system("bullets", std::move(bullets));

    /** INITIALIZE SHADERS **/
    sf::Shader bloom;
    if(!bloom.loadFromFile("././shaders/bloom.frag", sf::Shader::Type::Fragment))
    {
        std::cerr << "FAILED TO LOAD SHADER: bloom.frag" << std::endl;
    }

    float intensity = 2.0f; // Good default value
    float threshold = 0.1; // Good default value
    bloom.setUniform("texture", sf::Shader::CurrentTexture);
    bloom.setUniform("threshold", threshold);
    bloom.setUniform("intensity", intensity);

    m_shader_manager.add_shader("bloom", std::move(bloom));

}

void Game::process_events()
{
    // Using the mouse position relative to the window, scale it to the resolution scale, then
    // convert it to pixel coordinates relative to the RenderTexture gotten from the camera view
    sf::Vector2i pixel_pos = sf::Mouse::getPosition(m_window);
    sf::Vector2f scaled_pos = {
        static_cast<float>(pixel_pos.x) / m_resolution_scale.x,
        static_cast<float>(pixel_pos.y) / m_resolution_scale.y
    };
    sf::Vector2f world_pos = m_whole_screen_texture.mapPixelToCoords(sf::Vector2i(scaled_pos), m_camera.get_view());
    
    while(const std::optional event = m_window.pollEvent())
    {
        if(event->is<sf::Event::Closed>())
        {
            m_window.close();
        }

        for(const auto& widget : m_widgets.get_widgets())
        {
            widget->handle_event(m_window, world_pos);
        }
        
        if (const auto* keyPressed = event->getIf<sf::Event::KeyPressed>())
        {
            // Put whatever you want in here
            if(sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Enter))
            {
                m_widgets.get_widget<Frame>("frame")->toggle_moveability();
            }
            if(sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Num1))
            {
                m_widgets.get_widget<Button>("button")->set_position({256, 128});
            }
            if(sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Num2))
            {
                m_widgets.get_widget<Button>("button")->set_position({900, 400});
            }

            // Camera movements
            if(sf::Keyboard::isKeyPressed(sf::Keyboard::Key::A))
            {
                m_camera.set_transition(TransitionFunction::None);
                m_camera.move(sf::Vector2f(-1, 0));
                m_camera.set_transition(TransitionFunction::Linear);
            }
            if(sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D))
            {
                m_camera.set_transition(TransitionFunction::None);
                m_camera.move(sf::Vector2f(1, 0));
                m_camera.set_transition(TransitionFunction::Linear);
            }
            if(sf::Keyboard::isKeyPressed(sf::Keyboard::Key::W))
            {
                m_camera.set_transition(TransitionFunction::None);
                m_camera.move(sf::Vector2f(0, -1));
                m_camera.set_transition(TransitionFunction::Linear);
            }
            if(sf::Keyboard::isKeyPressed(sf::Keyboard::Key::S))
            {
                m_camera.set_transition(TransitionFunction::None);
                m_camera.move(sf::Vector2f(0, 1));
                m_camera.set_transition(TransitionFunction::Linear);
            }
            
            // Sample of firing a machine gun
            if(sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Space))
            {
                m_camera.shake(2);
                m_particle_system_manager.get_particle_system("bullets")->add_particles(1, sf::Color(255, 255, 255), Vector{30, sf::degrees(180)}, 3);
            }

        }

        if(m_widgets.get_widget<Frame>("frame")->get_widget<Button>("exit")->is_pressed())
        {
            m_particle_system_manager.get_particle_system("bp")->add_particles(20, sf::Color::White, Vector{2, sf::degrees(0)}, 180);
        }

        if(const auto* mousePressed = event->getIf<sf::Event::MouseButtonPressed>())
        {
            if(m_widgets.get_widget<Button>("button")->is_pressed())
            {
                m_camera.shake(50);
                m_particle_system_manager.get_particle_system("bp")->add_particles(30, sf::Color(203, 53, 61), Vector{4, sf::degrees(0)}, 180);
                m_particle_system_manager.get_particle_system("bp")->add_particles(30, sf::Color(249, 182, 78), Vector{3, sf::degrees(0)}, 180);
                m_particle_system_manager.get_particle_system("bp")->add_particles(30, sf::Color(237, 98, 64), Vector{5, sf::degrees(0)}, 180);
                m_particle_system_manager.get_particle_system("bp")->add_particles(20, sf::Color(86, 61, 67), Vector{1, sf::degrees(0)}, 180);
                m_particle_system_manager.get_particle_system("bp")->add_particles(20, sf::Color(106, 74, 87), Vector{2, sf::degrees(0)}, 180);
            }
        }
    }
}

void Game::update(sf::Time delta_time)
{
    float delta_time_seconds = delta_time.asSeconds() * 10;

    for(const auto& widget : m_widgets.get_widgets())
    {
        widget->update(delta_time);
    }

    m_particle_system_manager.set_particle_system_position("bp", m_widgets.get_widget<Button>("button")->get_position());
    m_particle_system_manager.update(delta_time, m_camera.get_position());
    m_camera.update();

}

void Game::render()
{
    m_window.clear(sf::Color(0, 0, 0));
    m_whole_screen_texture.clear(sf::Color(50, 50, 50));
    m_whole_screen_texture.setView(m_camera.get_view());

    // Render all widgets
    for(const auto& widget : m_widgets.get_widgets())
    {
        m_whole_screen_texture.draw(*widget);
    }

    // Render all particle systems added to manager
    m_whole_screen_texture.draw(m_particle_system_manager, m_shader_manager.get_shader("bloom").get());
    
    m_whole_screen_texture.display();

    m_whole_screen_sprite.setTexture(m_whole_screen_texture.getTexture());
    m_window.draw(m_whole_screen_sprite);
    m_window.display();
}

Game::Game() :
    m_whole_screen_texture(m_base_resolution), 
    m_whole_screen_sprite(m_whole_screen_texture.getTexture()),
    m_camera(sf::Vector2f(WINDOW_WIDTH, WINDOW_HEIGHT), sf::Vector2f(float(WINDOW_WIDTH)/2.f, float(WINDOW_HEIGHT)/2.f)),
    m_particle_system_manager(m_camera.get_position())
{
    // ===== DO NOT REMOVE FUNCTION CALLS ===== //
    m_context_settings.antiAliasingLevel = 0;
    m_window.create(sf::VideoMode({SCALED_WIDTH, SCALED_HEIGHT}), GAME_NAME, sf::Style::Default, sf::State::Windowed, m_context_settings); 
    
    m_whole_screen_texture.setView(m_camera.get_view());
    m_camera.set_transition(TransitionFunction::Linear);
    m_camera.set_transition_duration(0.07);

    m_whole_screen_texture.setSmooth(false);
    m_whole_screen_sprite.setScale(m_resolution_scale);
    m_whole_screen_sprite.setPosition({0, 0});
    // ======================================== //
}

Game::~Game() {}

void Game::run(int frame_per_seconds)
{
    // RUN USES THE FIXED TIME STEPS APPROACH
    sf::Time time_since_last_update = sf::Time::Zero;
    sf::Time time_per_frame = sf::seconds(1.f / frame_per_seconds);

    initialize_objects();

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

