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
    sf::Font font("././assets/fonts/default-font.ttf");

    /** INITIALIZE GUI WIDGETS **/

    m_widgets.add_widget("button", std::make_shared<Button>(sf::Vector2f(100, 50), sf::Color::Green));
    m_widgets.get_widget<Button>("button")->set_position(sf::Vector2f(600, 480));
    m_widgets.get_widget<Button>("button")->set_outline(sf::Color::White, 1);
    m_widgets.get_widget<Button>("button")->add_text("Click Me!", font, sf::Color::Black);
    m_widgets.get_widget<Button>("button")->set_transition(TransitionFunction::EaseOutElastic);

    m_widgets.add_widget("frame", std::make_shared<Frame>(sf::Vector2f(300, 300), sf::Color::Blue, font));
    m_widgets.get_widget<Frame>("frame")->set_position(sf::Vector2f(50, 50));
    m_widgets.get_widget<Frame>("frame")->set_outline(sf::Color::Black, 2);
    m_widgets.get_widget<Frame>("frame")->add_taskbar(20, sf::Color::White, sf::Color::Black, 2, "Big Frame");
    m_widgets.get_widget<Frame>("frame")->set_z_value(10);

    /** INITIALIZE PARTICLE SYSTEMS **/

    sf::Vector2f pos = m_widgets.get_widget<Button>("button")->get_position();
    ParticleSystem ps1(pos);
    ps1.toggle_gravity();
    ps1.toggle_fade();
    ps1.set_drag(0.99f);
    ps1.set_lifespan(sf::seconds(4));

    m_particle_system_manager.add_particle_system("bp", std::move(ps1));

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
    while(const std::optional event = m_window.pollEvent())
    {
        if(event->is<sf::Event::Closed>())
        {
            m_window.close();
        }

        for(const auto& widget : m_widgets.get_widgets())
        {
            widget->handle_event(m_window);
        }
        if (const auto* keyPressed = event->getIf<sf::Event::KeyPressed>())
        {
            // Put whatever you want in here
            m_widgets.get_widget<Button>("button")->set_position({100, 100});

        }
        if(const auto* mousePressed = event->getIf<sf::Event::MouseButtonPressed>())
        {
            if(m_widgets.get_widget<Button>("button")->is_pressed())
            {
                m_particle_system_manager.get_particle_system("bp")->add_particles(20, sf::Color::Green, Vector{4, sf::degrees(0)}, 180);
                m_particle_system_manager.get_particle_system("bp")->add_particles(20, sf::Color::Yellow, Vector{6, sf::degrees(0)}, 180);
                m_particle_system_manager.get_particle_system("bp")->add_particles(20, sf::Color::Blue, Vector{8, sf::degrees(0)}, 180);
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

    sf::Vector2i pixelPos = sf::Mouse::getPosition(m_window);
    sf::Vector2f worldPos = m_window.mapPixelToCoords(pixelPos);
    
    m_particle_system_manager.get_particle_system("bp")->set_position(m_widgets.get_widget<Button>("button")->get_position());
    m_particle_system_manager.update(delta_time);
                
}

void Game::render()
{
    m_window.clear(sf::Color(50, 50, 50));

    // Render all widgets
    for(const auto& widget : m_widgets.get_widgets())
    {
        m_window.draw(*widget);
    }

    // Render all particle systems added to manager
    m_window.draw(m_particle_system_manager, m_shader_manager.get_shader("bloom").get());

    m_window.display();
}

Game::Game()
{
    // ===== DO NOT REMOVE FUNCTION CALLS ===== //
    m_window.create(sf::VideoMode({WINDOW_WIDTH, WINDOW_HEIGHT}), GAME_NAME);
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

