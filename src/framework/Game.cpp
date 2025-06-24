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

    /**** OLD WAY OF MAKING WIDGETS ****
    auto button = std::make_unique<Button>(sf::Vector2f(100, 50), sf::Color::Green);
    button->set_position(sf::Vector2f(600, 480));
    button->set_outline(sf::Color::White, 1);
    button->add_text("Click Me!", font, sf::Color::Black);
    button->set_callback(print);
    m_widgets.push_back(std::move(button));
    */

    m_widgets.add_widget("button", std::make_shared<Button>(sf::Vector2f(100, 50), sf::Color::Green));
    m_widgets.get_widget<Button>("button")->set_position(sf::Vector2f(600, 480));
    m_widgets.get_widget<Button>("button")->set_outline(sf::Color::White, 1);
    m_widgets.get_widget<Button>("button")->add_text("Click Me!", font, sf::Color::Black);

    m_widgets.add_widget("slider", std::make_shared<Slider>(sf::Vector2f(200, 50), sf::Color::White));
    m_widgets.get_widget<Slider>("slider")->set_position(sf::Vector2f(WINDOW_WIDTH-300, 100));
    m_widgets.get_widget<Slider>("slider")->set_thumb_color(sf::Color::Yellow);

    m_widgets.add_widget("frame", std::make_shared<Frame>(sf::Vector2f(300, 300), sf::Color::Blue, font));
    m_widgets.get_widget<Frame>("frame")->set_position(sf::Vector2f(50, 50));
    m_widgets.get_widget<Frame>("frame")->set_outline(sf::Color::Black, 2);
    m_widgets.get_widget<Frame>("frame")->add_taskbar(20, sf::Color::White, sf::Color::Black, 2, "Big Frame");
    m_widgets.get_widget<Frame>("frame")->set_z_value(10);

    m_widgets.add_widget("gauge", std::make_shared<Gauge>(sf::Vector2f(100, 100), sf::Color::Green,
                sf::Vector2f(0, 80), sf::Vector2f(1, 360)));
    m_widgets.get_widget<Gauge>("gauge")->set_position(sf::Vector2f(100, WINDOW_HEIGHT-200));
    m_widgets.get_widget<Gauge>("gauge")->set_dial_color(sf::Color::White);

    m_widgets.add_widget("textbox", std::make_shared<TextBox>(sf::Vector2f(200, 100), sf::Color::White,
                "I am a textbox\nand I have a\nsick ass animation", font, sf::Color::Black));
    m_widgets.get_widget<TextBox>("textbox")->set_position(sf::Vector2f(WINDOW_WIDTH-300, WINDOW_HEIGHT-200));
    m_widgets.get_widget<TextBox>("textbox")->add_text_scroll(20);
    m_widgets.get_widget<TextBox>("textbox")->set_z_value(5);

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
            if(sf::Keyboard::isKeyPressed(sf::Keyboard::Key::G))
            {
                ps.toggle_gravity();
                std::cout << "Gravity" << std::endl;
            }
            if(sf::Keyboard::isKeyPressed(sf::Keyboard::Key::F))
            {
                ps.toggle_fade();
                std::cout << "Fade" << std::endl;
            }
            if(sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D))
            {
                ps.apply_force(Vector{5, sf::degrees(0)});
                std::cout << "Right" << std::endl;
            }
            if(sf::Keyboard::isKeyPressed(sf::Keyboard::Key::A))
            {
                ps.apply_force(Vector{5, sf::degrees(180)});
                std::cout << "Left" << std::endl;
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

    float p = m_widgets.get_widget<Slider>("slider")->get_percentage();
    m_widgets.get_widget<Gauge>("gauge")->set_arm_angle_based_on_value(80*p);

    sf::Vector2i pixelPos = sf::Mouse::getPosition(m_window);

    sf::Vector2f worldPos = m_window.mapPixelToCoords(pixelPos);

    ps.set_position(worldPos);
    if(sf::Mouse::isButtonPressed(sf::Mouse::Button::Left))
        ps.add_particles(1, sf::Color::Yellow, Vector{8, sf::degrees(0)}, 180);

    ps.update(delta_time);
}

void Game::render()
{
    m_window.clear(sf::Color(50, 50, 50));

    for(const auto& widget : m_widgets.get_widgets())
    {
        m_window.draw(*widget);
    }

    m_window.draw(ps);

    m_window.display();
}

Game::Game() : ps(sf::Vector2f(400, 400))
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

