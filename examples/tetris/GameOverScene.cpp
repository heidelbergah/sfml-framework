/**
 * GameOverScene.cpp  -  Cascade
 */

#include "GameOverScene.hpp"

#include <cstdio>

namespace tetris
{

namespace
{
    const std::string FONT_PATH = "assets/fonts/default.ttf";

    constexpr float VIEW_W = static_cast<float>(WINDOW_WIDTH);
    constexpr float VIEW_H = static_cast<float>(WINDOW_HEIGHT);
}

GameOverScene::GameOverScene()
    : m_ambient({VIEW_W, VIEW_H}, 70)
{
    m_font = Assets::font(FONT_PATH);

    // Slow settling motes, like dust after a collapse.
    ParticleSystem settle;
    settle.set_lifespan(sf::seconds(4.5f));
    settle.set_drag(0.75f);
    settle.set_fade(true);
    settle.set_gravity(sf::Vector2f{0.0f, 16.0f});
    settle.set_gravity_enabled(true);
    m_particles.add_particle_system("settle", std::move(settle));

    build_widgets();
}

void GameOverScene::build_widgets()
{
    if(!m_font) return;

    m_headline = std::make_shared<TextBox>(
        sf::Vector2f{VIEW_W, 46.0f}, sf::Color::Transparent,
        "TOPPED OUT", *m_font, RED);
    m_headline->set_character_size(24);
    m_widgets.add_widget("headline", m_headline);

    m_score = std::make_shared<TextBox>(
        sf::Vector2f{VIEW_W, 52.0f}, sf::Color::Transparent,
        "0", *m_font, FG0);
    m_score->set_character_size(34);
    m_widgets.add_widget("score", m_score);

    m_detail = std::make_shared<TextBox>(
        sf::Vector2f{VIEW_W, 70.0f}, sf::Color::Transparent,
        "", *m_font, FG3);
    m_detail->set_character_size(12);
    m_widgets.add_widget("detail", m_detail);

    m_record = std::make_shared<TextBox>(
        sf::Vector2f{VIEW_W, 24.0f}, sf::Color::Transparent,
        "", *m_font, YELLOW);
    m_record->set_character_size(13);
    m_widgets.add_widget("record", m_record);

    m_again = std::make_shared<Button>(sf::Vector2f{170.0f, 38.0f}, AQUA_DIM);
    m_again->add_text("PLAY AGAIN", *m_font, BG0_HARD);
    m_again->set_character_size(13);
    m_again->set_state_tints(-24, -44);
    m_again->set_callback([]() { EventBus::publish(Event(EventType::SWITCH_TO_GAME)); });
    m_widgets.add_widget("again", m_again);

    m_menu = std::make_shared<Button>(sf::Vector2f{170.0f, 30.0f}, BG2);
    m_menu->add_text("MENU", *m_font, FG2);
    m_menu->set_character_size(12);
    m_menu->set_state_tints(-18, -32);
    m_menu->set_callback([]() { EventBus::publish(Event(EventType::SWITCH_TO_MAIN_MENU)); });
    m_widgets.add_widget("menu", m_menu);
}

void GameOverScene::on_enter()
{
    GameTime::resume();

    GameState const& state = game_state();
    char buffer[160];

    if(m_score)
    {
        std::snprintf(buffer, sizeof(buffer), "%d", state.score);
        m_score->set_string(buffer);
    }

    if(m_detail)
    {
        std::snprintf(buffer, sizeof(buffer),
                      "LINES  %d          LEVEL  %d          PIECES  %d\n\n"
                      "TETRISES  %d       T-SPINS  %d       BEST COMBO  %d",
                      state.lines, state.level, state.pieces_placed,
                      state.tetrises, state.tspins,
                      std::max(0, state.best_combo + (state.best_combo > 0 ? 1 : 0)));
        m_detail->set_string(buffer);
        m_detail->add_text_scroll(80.0f);
    }

    if(m_record)
    {
        if(state.score >= state.best_score && state.score > 0)
            m_record->set_string("NEW BEST");
        else
        {
            std::snprintf(buffer, sizeof(buffer), "BEST  %d", state.best_score);
            m_record->set_string(buffer);
        }
    }

    m_settle_timer = 0.0f;

    struct Entry { std::shared_ptr<Widget> widget; float y; float duration; float width; };

    Entry const entries[] = {
        {m_headline,  60.0f,  0.5f,  VIEW_W},
        {m_score,     104.0f, 0.58f, VIEW_W},
        {m_detail,    172.0f, 0.66f, VIEW_W},
        {m_record,    262.0f, 0.74f, VIEW_W},
        {m_again,     318.0f, 0.82f, 170.0f},
        {m_menu,      368.0f, 0.90f, 170.0f}
    };

    for(const Entry& entry : entries)
    {
        if(!entry.widget) continue;

        float const x = (VIEW_W - entry.width) / 2.0f;

        entry.widget->set_position({x, entry.y - 250.0f}, true);
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
        if(key->code == sf::Keyboard::Key::Enter ||
           key->code == sf::Keyboard::Key::Space)
            EventBus::publish(Event(EventType::SWITCH_TO_GAME));
    }
}

void GameOverScene::update(sf::Time delta_time)
{
    m_widgets.update(delta_time);
    m_ambient.update(delta_time, 0.1f);

    m_settle_timer -= delta_time.asSeconds();
    if(m_settle_timer <= 0.0f)
    {
        m_settle_timer = 0.16f;

        if(auto settle = m_particles.get_particle_system("settle"))
        {
            settle->set_position({Random::real(0.0f, VIEW_W), -8.0f});
            settle->add_particles_varied(1, mix(BG4, RED_DIM, Random::real(0.0f, 0.5f)),
                                         Vector{14.0f, sf::degrees(270.0f)}, 30.0f, 6.0f);
        }
    }

    m_particles.update(delta_time);
}

void GameOverScene::render(sf::RenderTarget& target)
{
    target.setView(target.getDefaultView());

    m_ambient.draw(target);
    target.draw(m_particles);
    target.draw(m_widgets);
}

sf::Color GameOverScene::get_clear_color() const
{
    return sf::Color(0x1d, 0x18, 0x18);
}

}
