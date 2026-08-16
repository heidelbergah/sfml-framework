/**
 * MenuScene.cpp  -  Void Drift
 */

#include "../../include/scenes/MenuScene.hpp"
#include "../../include/RunState.hpp"

MenuScene::MenuScene() {}

MenuScene::~MenuScene()
{
    delete m_title;
    delete m_tagline;
    delete m_controls;
    delete m_hiscore;
}

void MenuScene::on_enter()
{
    if(!m_font.openFromFile("assets/fonts/Windows-Regular.ttf"))
        m_font.openFromFile("././assets/fonts/Windows-Regular.ttf");
    m_font.setSmooth(false);

    m_pulse = 0.f;

    m_title = new sf::Text(m_font, "VOID DRIFT", 36);
    m_title->setFillColor(sf::Color(80, 200, 255));
    m_title->setOutlineColor(sf::Color(0, 60, 120));
    m_title->setOutlineThickness(2.f);
    auto tb = m_title->getLocalBounds();
    m_title->setOrigin({tb.size.x / 2.f, tb.size.y / 2.f});
    m_title->setPosition({(float)WINDOW_WIDTH / 2.f, (float)WINDOW_HEIGHT / 3.f - 10.f});

    m_tagline = new sf::Text(m_font, "AN ASTEROIDS ROGUELITE", 10);
    m_tagline->setFillColor(sf::Color(120, 180, 220));
    auto lb = m_tagline->getLocalBounds();
    m_tagline->setOrigin({lb.size.x / 2.f, lb.size.y / 2.f});
    m_tagline->setPosition({(float)WINDOW_WIDTH / 2.f, (float)WINDOW_HEIGHT / 3.f + 22.f});

    m_hiscore = new sf::Text(m_font, "HI-SCORE: " + std::to_string(RunState::hi_score), 11);
    m_hiscore->setFillColor(sf::Color(255, 220, 80));
    auto hb = m_hiscore->getLocalBounds();
    m_hiscore->setOrigin({hb.size.x / 2.f, hb.size.y / 2.f});
    m_hiscore->setPosition({(float)WINDOW_WIDTH / 2.f, (float)WINDOW_HEIGHT / 3.f + 40.f});

    m_controls = new sf::Text(m_font,
        "A/D or Left/Right: Rotate     W or Up: Thrust     Space: Shoot", 8);
    m_controls->setFillColor(sf::Color(100, 150, 180));
    auto cb = m_controls->getLocalBounds();
    m_controls->setOrigin({cb.size.x / 2.f, cb.size.y / 2.f});
    m_controls->setPosition({(float)WINDOW_WIDTH / 2.f, (float)WINDOW_HEIGHT * 0.72f});

    // Play button
    m_widgets.add_widget("play",
        std::make_shared<Button>(sf::Vector2f(120.f, 28.f), sf::Color(30, 140, 200)));
    auto btn = m_widgets.get_widget<Button>("play");
    btn->set_position({(float)WINDOW_WIDTH / 2.f - 60.f, (float)WINDOW_HEIGHT / 2.f - 10.f}, true);
    btn->add_text("PLAY", m_font, sf::Color(10, 30, 50));
    btn->set_outline(sf::Color(80, 210, 255), 1);
    btn->set_transition(TransitionFunction::EaseOutElastic);
    btn->set_transition_duration(0.5f);
    btn->set_callback([](){ EventBus::publish(Event(EventType::SWITCH_TO_GAME, "")); });

    m_to_game_handle = EventBus::subscribe(EventType::SWITCH_TO_GAME, [](const Event&){});
}

void MenuScene::on_exit()
{
    EventBus::unsubscribe(EventType::SWITCH_TO_GAME, m_to_game_handle);
    m_widgets.remove_widget("play");
    delete m_title;    m_title    = nullptr;
    delete m_tagline;  m_tagline  = nullptr;
    delete m_controls; m_controls = nullptr;
    delete m_hiscore;  m_hiscore  = nullptr;
}

void MenuScene::process_events()
{
    sf::Vector2i px  = sf::Mouse::getPosition(*m_render_window);
    sf::Vector2f sp  = {(float)px.x / m_resolution_scale.x, (float)px.y / m_resolution_scale.y};
    sf::Vector2f wp  = m_render_texture->mapPixelToCoords(sf::Vector2i(sp));

    while(const std::optional ev = m_render_window->pollEvent())
    {
        if(ev->is<sf::Event::Closed>()) m_render_window->close();
        if(ev->getIf<sf::Event::MouseButtonPressed>() || ev->getIf<sf::Event::MouseMoved>())
            for(const auto& w : m_widgets.get_widgets())
                w->handle_event(*m_render_window, wp);
    }
}

void MenuScene::update(sf::Time dt)
{
    m_pulse += dt.asSeconds();
    m_stars.update(dt);
    for(const auto& w : m_widgets.get_widgets()) w->update(dt);

    // Update hi-score text in case a run just ended
    if(m_hiscore)
    {
        m_hiscore->setString("HI-SCORE: " + std::to_string(RunState::hi_score));
        auto hb = m_hiscore->getLocalBounds();
        m_hiscore->setOrigin({hb.size.x / 2.f, hb.size.y / 2.f});
    }

    float p = (std::sin(m_pulse * 1.8f) + 1.f) / 2.f;
    if(m_title) m_title->setFillColor(sf::Color(
        (uint8_t)(50  + p * 60),
        (uint8_t)(160 + p * 50),
        255));
}

void MenuScene::render()
{
    m_render_texture->clear(sf::Color(4, 4, 16));
    m_stars.draw(*m_render_texture);
    if(m_title)    m_render_texture->draw(*m_title);
    if(m_tagline)  m_render_texture->draw(*m_tagline);
    if(m_hiscore)  m_render_texture->draw(*m_hiscore);
    if(m_controls) m_render_texture->draw(*m_controls);
    for(const auto& w : m_widgets.get_widgets()) m_render_texture->draw(*w);
}
