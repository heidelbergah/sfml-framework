/**
 * MenuScene.cpp  -  Cascade
 */

#include "MenuScene.hpp"

#include <cstdio>

namespace tetris
{

namespace
{
    const std::string FONT_PATH = "assets/fonts/default.ttf";

    constexpr float VIEW_W = static_cast<float>(WINDOW_WIDTH);
    constexpr float VIEW_H = static_cast<float>(WINDOW_HEIGHT);
}

MenuScene::MenuScene()
    : m_ambient({VIEW_W, VIEW_H}, 110)
{
    m_font = Assets::font(FONT_PATH);

    for(int i = 0; i < 11; ++i) spawn_drifter(true);

    build_widgets();
}

void MenuScene::spawn_drifter(bool anywhere)
{
    Drifter drifter;
    drifter.type        = static_cast<PieceType>(Random::integer(0, PIECE_COUNT - 1));
    drifter.cell        = Random::real(11.0f, 21.0f);
    drifter.speed       = Random::real(9.0f, 24.0f);
    drifter.spin_period = Random::real(2.4f, 5.5f);
    drifter.rotation    = Random::integer(0, 3);
    drifter.alpha       = Random::real(0.05f, 0.14f);

    drifter.pos = { Random::real(-40.0f, VIEW_W + 40.0f),
                    anywhere ? Random::real(-40.0f, VIEW_H + 40.0f)
                             : VIEW_H + 60.0f };

    m_drifters.push_back(drifter);
}

void MenuScene::build_widgets()
{
    if(!m_font) return;

    m_title = std::make_shared<TextBox>(
        sf::Vector2f{VIEW_W, 58.0f}, sf::Color::Transparent,
        "C A S C A D E", *m_font, FG0);
    m_title->set_character_size(30);
    m_widgets.add_widget("title", m_title);

    m_subtitle = std::make_shared<TextBox>(
        sf::Vector2f{VIEW_W, 24.0f}, sf::Color::Transparent,
        "stack, clear, repeat", *m_font, GRAY);
    m_subtitle->set_character_size(12);
    m_widgets.add_widget("subtitle", m_subtitle);

    m_controls = std::make_shared<TextBox>(
        sf::Vector2f{440.0f, 96.0f}, BG0,
        "MOVE  arrows / A D          SOFT DROP  down / S\n"
        "ROTATE  up / X / Z          HARD DROP  space\n"
        "HOLD  C / shift             PAUSE  P    QUIT  esc",
        *m_font, FG3);
    m_controls->set_character_size(11);
    m_controls->set_outline(BG2, 1.0f);
    m_widgets.add_widget("controls", m_controls);

    m_best = std::make_shared<TextBox>(
        sf::Vector2f{VIEW_W, 24.0f}, sf::Color::Transparent,
        "", *m_font, YELLOW);
    m_best->set_character_size(12);
    m_widgets.add_widget("best", m_best);

    m_play = std::make_shared<Button>(sf::Vector2f{170.0f, 40.0f}, AQUA_DIM);
    m_play->add_text("PLAY", *m_font, BG0_HARD);
    m_play->set_character_size(15);
    m_play->set_state_tints(-24, -44);      // brighten on hover instead
    m_play->set_callback([]() { EventBus::publish(Event(EventType::SWITCH_TO_GAME)); });
    m_widgets.add_widget("play", m_play);

    m_quit = std::make_shared<Button>(sf::Vector2f{170.0f, 30.0f}, BG2);
    m_quit->add_text("QUIT", *m_font, FG2);
    m_quit->set_character_size(12);
    m_quit->set_state_tints(-18, -32);
    m_quit->set_callback([]() { EventBus::publish(Event(EventType::QUIT)); });
    m_widgets.add_widget("quit", m_quit);
}

void MenuScene::on_enter()
{
    GameTime::resume();

    GameState const& state = game_state();
    if(m_best)
    {
        if(state.best_score > 0)
        {
            char buffer[80];
            std::snprintf(buffer, sizeof(buffer), "BEST  %d       LINES  %d",
                          state.best_score, state.best_lines);
            m_best->set_string(buffer);
        }
        else
        {
            m_best->set_string("");
        }
    }

    struct Entry { std::shared_ptr<Widget> widget; float y; float duration; float width; };

    Entry const entries[] = {
        {m_title,     92.0f,  0.55f, VIEW_W},
        {m_subtitle,  144.0f, 0.62f, VIEW_W},
        {m_controls,  196.0f, 0.70f, 440.0f},
        {m_play,      320.0f, 0.78f, 170.0f},
        {m_quit,      372.0f, 0.86f, 170.0f},
        {m_best,      432.0f, 0.94f, VIEW_W}
    };

    for(const Entry& entry : entries)
    {
        if(!entry.widget) continue;

        float const x = (VIEW_W - entry.width) / 2.0f;

        entry.widget->set_position({x, entry.y - 280.0f}, true);
        entry.widget->set_transition(TransitionFunction::EaseOutBack);
        entry.widget->set_transition_duration(entry.duration);
        entry.widget->set_position({x, entry.y});
    }
}

void MenuScene::begin_frame()
{
    m_widgets.begin_frame();
}

void MenuScene::handle_event(const sf::Event& event, sf::Vector2f mouse_pos)
{
    m_widgets.handle_event(event, mouse_pos);

    if(const auto* key = event.getIf<sf::Event::KeyPressed>())
    {
        if(key->code == sf::Keyboard::Key::Escape)
            EventBus::publish(Event(EventType::QUIT));

        if(key->code == sf::Keyboard::Key::Enter ||
           key->code == sf::Keyboard::Key::Space)
            EventBus::publish(Event(EventType::SWITCH_TO_GAME));
    }
}

void MenuScene::update(sf::Time delta_time)
{
    float const dt = delta_time.asSeconds();

    m_widgets.update(delta_time);
    m_ambient.update(delta_time, 0.22f);

    m_backdrop.clear();

    for(Drifter& drifter : m_drifters)
    {
        drifter.pos.y -= drifter.speed * dt;

        drifter.spin_timer += dt;
        if(drifter.spin_timer >= drifter.spin_period)
        {
            drifter.spin_timer -= drifter.spin_period;
            drifter.rotation = (drifter.rotation + 1) % 4;
        }

        Cells const& cells = piece_cells(drifter.type, drifter.rotation);
        for(sf::Vector2i c : cells)
        {
            sf::Vector2f const at{drifter.pos.x + static_cast<float>(c.x) * drifter.cell,
                                  drifter.pos.y + static_cast<float>(c.y) * drifter.cell};
            push_block(m_backdrop, at, drifter.cell,
                       piece_color(drifter.type), drifter.alpha);
        }
    }

    // Recycle anything that has drifted off the top.
    std::erase_if(m_drifters, [](const Drifter& d) { return d.pos.y < -110.0f; });
    while(m_drifters.size() < 11) spawn_drifter(false);
}

void MenuScene::render(sf::RenderTarget& target)
{
    target.setView(target.getDefaultView());

    m_ambient.draw(target);
    if(m_backdrop.getVertexCount() > 0) target.draw(m_backdrop);

    target.draw(m_widgets);
}

sf::Color MenuScene::get_clear_color() const
{
    return BG0_HARD;
}

}
