/**
 * MenuScene.cpp  -  Ironvale Arena
 */

#include "MenuScene.hpp"

#include <cstdio>

namespace arena
{

namespace
{
    const std::string FONT_PATH = "assets/fonts/default.ttf";

    constexpr float VIEW_W = static_cast<float>(WINDOW_WIDTH);
    constexpr float VIEW_H = static_cast<float>(WINDOW_HEIGHT);
}

MenuScene::MenuScene()
    : m_camera({VIEW_W, VIEW_H}, {VIEW_W / 2.0f, VIEW_H / 2.0f})
{
    ParticleSystem embers;
    embers.set_lifespan(sf::seconds(4.0f));
    embers.set_drag(0.7f);
    embers.set_fade(true);
    embers.set_gravity(sf::Vector2f{0.0f, -14.0f});   // drifting upward
    embers.set_gravity_enabled(true);
    m_particles.add_particle_system("embers", std::move(embers));

    build_widgets();
}

void MenuScene::build_widgets()
{
    const sf::Font* font = Assets::font(FONT_PATH);
    if(!font) return;

    m_title = std::make_shared<TextBox>(
        sf::Vector2f{VIEW_W, 60.0f}, sf::Color::Transparent,
        "IRONVALE  ARENA", *font, sf::Color(236, 240, 250));
    m_title->set_character_size(28);
    m_widgets.add_widget("title", m_title);

    m_subtitle = std::make_shared<TextBox>(
        sf::Vector2f{VIEW_W, 26.0f}, sf::Color::Transparent,
        "an SFML 3 framework demo", *font, sf::Color(132, 146, 178));
    m_subtitle->set_character_size(12);
    m_widgets.add_widget("subtitle", m_subtitle);

    m_controls = std::make_shared<TextBox>(
        sf::Vector2f{420.0f, 92.0f}, sf::Color(24, 27, 40),
        "WASD  drive and turn the hull\n"
        "MOUSE  aim the turret     LMB  fire\n"
        "P  pause      ESC  back to this menu",
        *font, sf::Color(178, 188, 210));
    m_controls->set_character_size(12);
    m_controls->set_outline(sf::Color(52, 60, 84), 2.0f);
    m_widgets.add_widget("controls", m_controls);

    m_best = std::make_shared<TextBox>(
        sf::Vector2f{VIEW_W, 24.0f}, sf::Color::Transparent,
        "", *font, sf::Color(226, 190, 120));
    m_best->set_character_size(12);
    m_widgets.add_widget("best", m_best);

    m_play = std::make_shared<Button>(sf::Vector2f{150.0f, 40.0f},
                                      sf::Color(96, 150, 122));
    m_play->add_text("DEPLOY", *font, sf::Color(16, 22, 20));
    m_play->set_character_size(14);
    m_play->set_callback([]()
    {
        // The menu does not know what scene comes next; it just says
        // what happened and lets SceneManager route it.
        EventBus::publish(Event(EventType::SWITCH_TO_GAME));
    });
    m_widgets.add_widget("play", m_play);

    m_quit = std::make_shared<Button>(sf::Vector2f{150.0f, 30.0f},
                                      sf::Color(72, 78, 100));
    m_quit->add_text("QUIT", *font, sf::Color(216, 222, 236));
    m_quit->set_character_size(12);
    m_quit->set_callback([]() { EventBus::publish(Event(EventType::QUIT)); });
    m_widgets.add_widget("quit", m_quit);
}

void MenuScene::on_enter()
{
    GameTime::resume();

    RunState const& state = run_state();
    if(m_best)
    {
        if(state.best_score > 0)
        {
            char buffer[64];
            std::snprintf(buffer, sizeof(buffer), "BEST  %d", state.best_score);
            m_best->set_string(buffer);
        }
        else
        {
            m_best->set_string("");
        }
    }

    /*
     * Everything flies in from off-screen on a staggered delay. The
     * tweens are unscaled so the menu animates even if the previous
     * scene left time stopped.
     */
    struct Entry { std::shared_ptr<Widget> widget; float y; float duration; };

    Entry const entries[] = {
        {m_title,    70.0f,  0.55f},
        {m_subtitle, 120.0f, 0.62f},
        {m_controls, 175.0f, 0.70f},
        {m_play,     300.0f, 0.78f},
        {m_quit,     352.0f, 0.86f},
        {m_best,     420.0f, 0.94f}
    };

    for(const Entry& entry : entries)
    {
        if(!entry.widget) continue;

        float const x = (entry.widget == m_controls) ? (VIEW_W - 420.0f) / 2.0f
                      : (entry.widget == m_play || entry.widget == m_quit)
                        ? (VIEW_W - 150.0f) / 2.0f
                        : 0.0f;

        entry.widget->set_position({x, entry.y - 260.0f}, true);
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
    m_widgets.update(delta_time);

    // Ambient embers drifting up from the bottom edge.
    m_ember_timer -= delta_time.asSeconds();
    if(m_ember_timer <= 0.0f)
    {
        m_ember_timer = 0.07f;

        if(auto embers = m_particles.get_particle_system("embers"))
        {
            embers->set_position({Random::real(0.0f, VIEW_W), VIEW_H + 8.0f});
            embers->add_particles_varied(1, sf::Color(206, 120, 84),
                                         Vector{34.0f, sf::degrees(90.0f)},
                                         26.0f, 12.0f);
        }
    }

    m_particles.update(delta_time);
    m_camera.update(delta_time);
}

void MenuScene::render(sf::RenderTarget& target)
{
    target.setView(target.getDefaultView());
    target.draw(m_particles);
    target.draw(m_widgets);
}

sf::Color MenuScene::get_clear_color() const
{
    return sf::Color(14, 16, 24);
}

}   // namespace arena
