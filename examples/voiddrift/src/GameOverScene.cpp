/**
 * GameOverScene.cpp  -  Void Drift
 */

#include "../../include/scenes/GameOverScene.hpp"

GameOverScene::GameOverScene() {}

GameOverScene::~GameOverScene()
{
    delete m_title;
    delete m_score_text;
    delete m_hi_text;
    delete m_perks_text;
}

void GameOverScene::on_enter()
{
    if(!m_font.openFromFile("assets/fonts/Windows-Regular.ttf"))
        m_font.openFromFile("././assets/fonts/Windows-Regular.ttf");
    m_font.setSmooth(false);

    m_pulse = 0.f;

    m_title = new sf::Text(m_font, "GAME OVER", 30);
    m_title->setFillColor(sf::Color(255, 80, 80));
    m_title->setOutlineColor(sf::Color(100, 10, 10));
    m_title->setOutlineThickness(2.f);
    auto tb = m_title->getLocalBounds();
    m_title->setOrigin({tb.size.x / 2.f, tb.size.y / 2.f});
    m_title->setPosition({(float)WINDOW_WIDTH / 2.f, (float)WINDOW_HEIGHT * 0.28f});

    m_score_text = new sf::Text(m_font,
        "SCORE: " + std::to_string(RunState::score) +
        "   LEVEL: " + std::to_string(RunState::level), 13);
    m_score_text->setFillColor(sf::Color(200, 240, 255));
    auto sb = m_score_text->getLocalBounds();
    m_score_text->setOrigin({sb.size.x / 2.f, sb.size.y / 2.f});
    m_score_text->setPosition({(float)WINDOW_WIDTH / 2.f, (float)WINDOW_HEIGHT * 0.28f + 32.f});

    m_hi_text = new sf::Text(m_font, "HI-SCORE: " + std::to_string(RunState::hi_score), 11);
    m_hi_text->setFillColor(sf::Color(255, 220, 80));
    auto hb = m_hi_text->getLocalBounds();
    m_hi_text->setOrigin({hb.size.x / 2.f, hb.size.y / 2.f});
    m_hi_text->setPosition({(float)WINDOW_WIDTH / 2.f, (float)WINDOW_HEIGHT * 0.28f + 52.f});

    // List perks earned this run
    std::string perk_str = "PERKS: ";
    if(RunState::perks.empty())
    {
        perk_str += "none";
    }
    else
    {
        for(int i = 0; i < (int)RunState::perks.size(); ++i)
        {
            for(const auto& pd : ALL_PERKS)
                if(pd.type == RunState::perks[i]) { perk_str += pd.name; break; }
            if(i < (int)RunState::perks.size() - 1) perk_str += ", ";
        }
    }
    m_perks_text = new sf::Text(m_font, perk_str, 8);
    m_perks_text->setFillColor(sf::Color(140, 180, 220));
    auto pb = m_perks_text->getLocalBounds();
    m_perks_text->setOrigin({pb.size.x / 2.f, pb.size.y / 2.f});
    m_perks_text->setPosition({(float)WINDOW_WIDTH / 2.f, (float)WINDOW_HEIGHT * 0.28f + 70.f});

    // Play again
    m_widgets.add_widget("retry",
        std::make_shared<Button>(sf::Vector2f(130.f, 28.f), sf::Color(180, 40, 40)));
    auto retry = m_widgets.get_widget<Button>("retry");
    retry->set_position({(float)WINDOW_WIDTH / 2.f - 65.f, (float)WINDOW_HEIGHT / 2.f + 10.f}, true);
    retry->add_text("PLAY AGAIN", m_font, sf::Color(255, 220, 220));
    retry->set_outline(sf::Color(255, 100, 100), 1);
    retry->set_transition(TransitionFunction::EaseOutElastic);
    retry->set_transition_duration(0.5f);
    retry->set_callback([](){
        RunState::full_reset();
        EventBus::publish(Event(EventType::SWITCH_TO_GAME, ""));
    });

    // Main menu
    m_widgets.add_widget("menu",
        std::make_shared<Button>(sf::Vector2f(130.f, 28.f), sf::Color(20, 80, 120)));
    auto menu = m_widgets.get_widget<Button>("menu");
    menu->set_position({(float)WINDOW_WIDTH / 2.f - 65.f, (float)WINDOW_HEIGHT / 2.f + 46.f}, true);
    menu->add_text("MAIN MENU", m_font, sf::Color(180, 220, 255));
    menu->set_outline(sf::Color(60, 180, 255), 1);
    menu->set_transition(TransitionFunction::EaseOutElastic);
    menu->set_transition_duration(0.55f);
    menu->set_callback([](){
        RunState::full_reset();
        EventBus::publish(Event(EventType::SWITCH_TO_MAIN_MENU, ""));
    });

    m_to_game_handle = EventBus::subscribe(EventType::SWITCH_TO_GAME,      [](const Event&){});
    m_to_menu_handle = EventBus::subscribe(EventType::SWITCH_TO_MAIN_MENU, [](const Event&){});
}

void GameOverScene::on_exit()
{
    EventBus::unsubscribe(EventType::SWITCH_TO_GAME,       m_to_game_handle);
    EventBus::unsubscribe(EventType::SWITCH_TO_MAIN_MENU,  m_to_menu_handle);
    m_widgets.remove_widget("retry");
    m_widgets.remove_widget("menu");
    delete m_title;      m_title      = nullptr;
    delete m_score_text; m_score_text = nullptr;
    delete m_hi_text;    m_hi_text    = nullptr;
    delete m_perks_text; m_perks_text = nullptr;
}

void GameOverScene::process_events()
{
    sf::Vector2f res_scale = {static_cast<float>(SCALE), static_cast<float>(SCALE)};
    sf::Vector2i px  = sf::Mouse::getPosition(*m_render_window);
    sf::Vector2f sp  = {(float)px.x / res_scale.x, (float)px.y / res_scale.y};
    sf::Vector2f wp  = m_render_texture->mapPixelToCoords(sf::Vector2i(sp));

    while(const std::optional ev = m_render_window->pollEvent())
    {
        if(ev->is<sf::Event::Closed>()) m_render_window->close();
        if(ev->getIf<sf::Event::MouseButtonPressed>() || ev->getIf<sf::Event::MouseMoved>())
            for(const auto& w : m_widgets.get_widgets())
                w->handle_event(*m_render_window, wp);
    }
}

void GameOverScene::update(sf::Time dt)
{
    m_pulse += dt.asSeconds();
    m_stars.update(dt);
    for(const auto& w : m_widgets.get_widgets()) w->update(dt);

    float p = (std::sin(m_pulse * 2.5f) + 1.f) / 2.f;
    if(m_title) m_title->setFillColor(sf::Color(
        (uint8_t)(200 + p * 55),
        (uint8_t)(30  + p * 30),
        (uint8_t)(30)));
}

void GameOverScene::render()
{
    m_render_texture->clear(sf::Color(8, 4, 4));
    m_stars.draw(*m_render_texture);
    if(m_title)      m_render_texture->draw(*m_title);
    if(m_score_text) m_render_texture->draw(*m_score_text);
    if(m_hi_text)    m_render_texture->draw(*m_hi_text);
    if(m_perks_text) m_render_texture->draw(*m_perks_text);
    for(const auto& w : m_widgets.get_widgets()) m_render_texture->draw(*w);
}
