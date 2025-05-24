/**
 * Game.cpp
 *
 * Written by James Bouchat
 */

#include "../../include/framework/Game.hpp"

void Game::initialize_objects()
{
    sf::Font font("././assets/fonts/default-font.ttf");

    // Buttons
    auto button = std::make_unique<Button>(sf::Vector2f(50, 50), sf::Color::Red);
    button->set_position(sf::Vector2f(20, 35));
    m_widgets.push_back(std::move(button));

    button = std::make_unique<Button>(sf::Vector2f(100, 50), sf::Color::Red);
    button->set_position(sf::Vector2f(600, 480));
    button->set_outline(sf::Color::White, 1);
    button->add_text("Hello World", font, sf::Color::Black);
    m_widgets.push_back(std::move(button));

    // Textboxes
    std::string text = "I am a textbox.\nFear me, as my swiftness\nand jurisdiction convicts you.\nI am inevitable.";
    auto text_box = std::make_unique<TextBox>(sf::Vector2f(300, 150),
            sf::Color::White, text, font, sf::Color::Black);
    text_box->set_position(sf::Vector2f(300, 300));
    text_box->set_outline(sf::Color::Black, 2);
    text_box->add_text_scroll(25);
    m_widgets.push_back(std::move(text_box));

    // Frames
    auto frame = std::make_unique<Frame>(sf::Vector2f(200, 200), sf::Color::White, font);
    frame->set_position(sf::Vector2f(0, 300));
    frame->set_outline(sf::Color::Black, 3);
    frame->add_taskbar(20, sf::Color::Cyan, sf::Color::Black, 3, "Taskbar");

    // Adding a button
    std::unique_ptr<Widget> framebutton = std::make_unique<Button>(sf::Vector2f(50, 50),
            sf::Color::Yellow);
    framebutton->set_position(sf::Vector2f(3, 0));
    frame->add_widget(std::move(framebutton));

    m_widgets.push_back(std::move(frame));
}

void Game::process_events()
{
    while(const std::optional event = m_window.pollEvent())
    {
        if(event->is<sf::Event::Closed>())
        {
            m_window.close();
        }

        for(const auto& widget : m_widgets)
        {
            widget->handle_event(m_window);
        }
    }
    
}

void Game::update(sf::Time delta_time)
{
    float delta_time_seconds = delta_time.asSeconds() * 10;

    for(const auto& widget : m_widgets)
    {
        widget->update(delta_time);
    }

}

void Game::render()
{
    m_window.clear(sf::Color(50, 50, 50));
    for(const auto& widget : m_widgets)
    {
        m_window.draw(*widget);
    }
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
