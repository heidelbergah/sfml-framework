/**
 * GameOverScene.cpp  -  Ironvale Arena
 */

#include "GameOverScene.hpp"

#include <cstdio>

namespace arena
{

namespace
{
    const std::string FONT_PATH = "assets/fonts/default.ttf";

    constexpr float VIEW_W = static_cast<float>(WINDOW_WIDTH);
    constexpr float VIEW_H = static_cast<float>(WINDOW_HEIGHT);
}

GameOverScene::GameOverScene()
{
    ParticleSystem ash;
    ash.set_lifespan(sf::seconds(6.0f));
    ash.set_drag(0.85f);
    ash.set_fade(true);
    ash.set_gravity(sf::Vector2f{0.0f, 22.0f});
    ash.set_gravity_enabled(true);
    m_particles.add_particle_system("ash", std::move(ash));

    build_widgets();
}

void GameOverScene::build_widgets()
{
    const sf::Font* font = Assets::font(FONT_PATH);
    if(!font) return;

    m_headline = std::make_shared<TextBox>(
        sf::Vector2f{VIEW_W, 52.0f}, sf::Color::Transparent,
        "RUN  ENDED", *font, sf::Color(232, 128, 118));
    m_headline->set_character_size(24);
    m_widgets.add_widget("headline", m_headline);

    m_summary = std::make_shared<TextBox>(
        sf::Vector2f{VIEW_W, 80.0f}, sf::Color::Transparent,
        "", *font, sf::Color(214, 220, 236));
    m_summary->set_character_size(12);
    m_widgets.add_widget("summary", m_summary);

    m_record = std::make_shared<TextBox>(
        sf::Vector2f{VIEW_W, 24.0f}, sf::Color::Transparent,
        "", *font, sf::Color(240, 200, 120));
    m_record->set_character_size(12);
    m_widgets.add_widget("record", m_record);

    m_retry = std::make_shared<Button>(sf::Vector2f{150.0f, 38.0f},
                                       sf::Color(96, 150, 122));
    m_retry->add_text("REDEPLOY", *font, sf::Color(16, 22, 20));
    m_retry->set_character_size(13);
    m_retry->set_callback([]() { EventBus::publish(Event(EventType::SWITCH_TO_GAME)); });
    m_widgets.add_widget("retry", m_retry);

    m_menu = std::make_shared<Button>(sf::Vector2f{150.0f, 30.0f},
                                      sf::Color(72, 78, 100));
    m_menu->add_text("MENU", *font, sf::Color(216, 222, 236));
    m_menu->set_character_size(12);
    m_menu->set_callback([]() { EventBus::publish(Event(EventType::SWITCH_TO_MAIN_MENU)); });
    m_widgets.add_widget("menu", m_menu);
}

void GameOverScene::on_enter()
{
    GameTime::resume();

    RunState const& state = run_state();

    if(m_summary)
    {
        char buffer[160];
        std::snprintf(buffer, sizeof(buffer),
                      "%s went down on wave %d\n\nSCORE  %d      KILLS  %d",
                      state.callsign.c_str(), state.wave, state.score, state.kills);
        m_summary->set_string(buffer);
        m_summary->add_text_scroll(45.0f);
    }

    if(m_record)
    {
        char buffer[64];
        if(state.score >= state.best_score && state.score > 0)
            std::snprintf(buffer, sizeof(buffer), "NEW BEST");
        else
            std::snprintf(buffer, sizeof(buffer), "BEST  %d", state.best_score);
        m_record->set_string(buffer);
    }

    struct Entry { std::shared_ptr<Widget> widget; float y; float duration; };

    Entry const entries[] = {
        {m_headline, 80.0f,  0.5f},
        {m_summary,  150.0f, 0.6f},
        {m_record,   250.0f, 0.68f},
        {m_retry,    310.0f, 0.76f},
        {m_menu,     360.0f, 0.84f}
    };

    for(const Entry& entry : entries)
    {
        if(!entry.widget) continue;

        float const x = (entry.widget == m_retry || entry.widget == m_menu)
                      ? (VIEW_W - 150.0f) / 2.0f : 0.0f;

        entry.widget->set_position({x, entry.y - 240.0f}, true);
        entry.widget->set_transition(TransitionFunction::EaseOutBack);
        entry.widget->set_transition_duration(entry.duration);
        entry.widget->set_position({x, entry.y});
    }
}

void GameOverScene::begin_frame()
{
    m_widgets.begin_frame();
}

void GameOverScene::handle_event(const sf::Event& event, sf::Vector2f mouse_pos)
{
    m_widgets.handle_event(event, mouse_pos);

    if(const auto* key = event.getIf<sf::Event::KeyPressed>())
    {
        if(key->code == sf::Keyboard::Key::Escape)
            EventBus::publish(Event(EventType::SWITCH_TO_MAIN_MENU));
        if(key->code == sf::Keyboard::Key::Enter)
            EventBus::publish(Event(EventType::SWITCH_TO_GAME));
    }
}

void GameOverScene::update(sf::Time delta_time)
{
    m_widgets.update(delta_time);

    m_ash_timer -= delta_time.asSeconds();
    if(m_ash_timer <= 0.0f)
    {
        m_ash_timer = 0.11f;
        if(auto ash = m_particles.get_particle_system("ash"))
        {
            ash->set_position({Random::real(0.0f, VIEW_W), -8.0f});
            ash->add_particles_varied(1, sf::Color(120, 112, 118),
                                      Vector{18.0f, sf::degrees(270.0f)}, 30.0f, 8.0f);
        }
    }

    m_particles.update(delta_time);
}

void GameOverScene::render(sf::RenderTarget& target)
{
    target.setView(target.getDefaultView());
    target.draw(m_particles);
    target.draw(m_widgets);
}

sf::Color GameOverScene::get_clear_color() const
{
    return sf::Color(20, 14, 18);
}

}
